#include "EscapeAnalysisPass.hpp"
#include "Dominance.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/ir/IRValue.hpp"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sph {

namespace {

bool canBasicBlockReachSelf(const IRBasicBlock* targetBB) {
    if (!targetBB) return false;
    std::unordered_set<const IRBasicBlock*> visited;
    std::vector<const IRBasicBlock*> worklist;
    for (const auto* succ : getSuccessors(*targetBB)) {
        if (succ) worklist.push_back(succ);
    }
    while (!worklist.empty()) {
        const auto* curr = worklist.back();
        worklist.pop_back();
        if (curr == targetBB) return true;
        if (visited.insert(curr).second) {
            for (const auto* succ : getSuccessors(*curr)) {
                if (succ) worklist.push_back(succ);
            }
        }
    }
    return false;
}

bool isPointerLikeType(const std::shared_ptr<Type>& t) {
    if (!t) return false;
    return t->isPointer() || t->isBox() || t->isStruct() || t->isClass() || t->isArena() || t->isVector();
}

enum class IterAllocState : uint8_t {
    Unreached = 0,
    Live = 1,
    Freed = 2,
    Unsafe = 3
};

IterAllocState mergeState(IterAllocState a, IterAllocState b) {
    if (a == IterAllocState::Unreached) return b;
    if (b == IterAllocState::Unreached) return a;
    if (a == b) return a;
    return IterAllocState::Unsafe;
}

bool verifyLoopLocalLifetime(const IRFunction& function,
                             const DominatorTree& domTree,
                             const NaturalLoop& innerLoop,
                             const std::vector<NaturalLoop>& allLoops,
                             const CallInst* allocVal,
                             const IRBasicBlock* allocBB,
                             const std::unordered_set<const IRValue*>& aliases,
                             const std::unordered_set<const IRValue*>& baseAliases,
                             const std::unordered_set<const IRInstruction*>& freeSet) {
    if (freeSet.empty()) return false;

    // 1. All instructions using or defining any alias must reside inside innerLoop,
    //    and allocBB must dominate every block containing an alias use.
    // 2. No PhiInst may carry an alias across a loop header or backedge.
    std::unordered_set<const IRBasicBlock*> loopHeaders;
    for (const auto& loop : allLoops) {
        if (loop.blocks.count(const_cast<IRBasicBlock*>(allocBB))) {
            loopHeaders.insert(loop.header);
        }
    }

    for (const auto& bb : function.getBlocks()) {
        if (!bb) continue;
        bool inInnerLoop = innerLoop.blocks.count(bb.get()) > 0;

        for (const auto& inst : bb->getInstructions()) {
            if (!inst || inst.get() == allocVal) continue;

            if (inst->getOpcode() == IROpcode::Phi) {
                const auto& phi = static_cast<const PhiInst&>(*inst);
                bool usesAlias = aliases.count(inst.get()) > 0;
                for (const auto& [inBb, inVal] : phi.getIncoming()) {
                    if (inVal && aliases.count(inVal.get())) {
                        usesAlias = true;
                        // Reject any Phi on a backedge or loop header
                        if (loopHeaders.count(bb.get()) || !inBb || domTree.dominates(bb.get(), inBb)) {
                            return false;
                        }
                    }
                }
                if (usesAlias && (!inInnerLoop || !domTree.dominates(allocBB, bb.get()))) {
                    return false;
                }
                continue;
            }

            bool touchesAlias = aliases.count(inst.get()) > 0;
            if (!touchesAlias) {
                for (const auto* op : getInstructionOperands(*inst)) {
                    if (op && aliases.count(op)) {
                        touchesAlias = true;
                        break;
                    }
                }
            }
            if (touchesAlias) {
                if (!inInnerLoop || !domTree.dominates(allocBB, bb.get())) {
                    return false;
                }
            }
        }
    }

    // 3. Forward intra-iteration state verification within innerLoop:
    //    Every path from allocVal inside innerLoop must execute a dominating sph_free
    //    before reaching any loop backedge, loop exit edge, or RetInst, with zero
    //    uses or double-frees after free.
    std::unordered_map<const IRBasicBlock*, IterAllocState> inState;
    for (auto* b : innerLoop.blocks) {
        inState[b] = IterAllocState::Unreached;
    }

    auto evaluateBlock = [&](const IRBasicBlock* bb, IterAllocState entryState, bool startAtAlloc) -> IterAllocState {
        IterAllocState cur = startAtAlloc ? IterAllocState::Unreached : entryState;
        bool active = !startAtAlloc;

        for (const auto& inst : bb->getInstructions()) {
            if (!inst) continue;

            if (startAtAlloc && inst.get() == allocVal) {
                cur = IterAllocState::Live;
                active = true;
                continue;
            }

            if (!active) continue;

            if (freeSet.count(inst.get())) {
                const auto& c = static_cast<const CallInst&>(*inst);
                if (c.getArgs().empty() || !baseAliases.count(c.getArgs()[0].get())) {
                    return IterAllocState::Unsafe;
                }
                if (cur != IterAllocState::Live) {
                    return IterAllocState::Unsafe; // Double-free or mixed state
                }
                cur = IterAllocState::Freed;
                continue;
            }

            // Check any alias use after free
            if (cur != IterAllocState::Live) {
                if (aliases.count(inst.get())) {
                    return IterAllocState::Unsafe;
                }
                for (const auto* op : getInstructionOperands(*inst)) {
                    if (op && aliases.count(op)) {
                        return IterAllocState::Unsafe;
                    }
                }
            }

            if (inst->getOpcode() == IROpcode::Ret && cur != IterAllocState::Freed) {
                return IterAllocState::Unsafe;
            }
        }

        return cur;
    };

    std::vector<const IRBasicBlock*> worklist;
    worklist.push_back(allocBB);
    std::unordered_set<const IRBasicBlock*> seenStart;

    while (!worklist.empty()) {
        const IRBasicBlock* bb = worklist.back();
        worklist.pop_back();

        bool isStart = (bb == allocBB) && seenStart.insert(bb).second;
        IterAllocState outState = evaluateBlock(bb, inState[bb], isStart);
        if (outState == IterAllocState::Unsafe) {
            return false;
        }
        if (outState == IterAllocState::Unreached) {
            continue;
        }

        for (const auto* succ : getSuccessors(*bb)) {
            if (!succ) continue;

            // Backedge to loop header OR exit outside innerLoop: allocation MUST be Freed!
            if (succ == innerLoop.header || !innerLoop.blocks.count(const_cast<IRBasicBlock*>(succ))) {
                if (outState != IterAllocState::Freed) {
                    return false;
                }
                continue;
            }

            // Re-entering allocBB inside the same iteration without crossing header would re-alloc
            if (succ == allocBB) {
                return false;
            }

            IterAllocState prev = inState[succ];
            IterAllocState next = mergeState(prev, outState);
            if (next == IterAllocState::Unsafe) {
                return false;
            }
            if (next != prev) {
                inState[succ] = next;
                worklist.push_back(succ);
            }
        }
    }

    return true;
}

} // namespace

bool EscapeAnalysisPass::runOnFunction(IRFunction& function) {
    if (function.getBlocks().empty()) return false;

    bool changed = false;

    std::vector<std::pair<std::shared_ptr<CallInst>, const IRBasicBlock*>> allocCandidates;

    // Step 1: Find all CallInst to "sph_alloc"
    for (const auto& bb : function.getBlocks()) {
        if (!bb) continue;
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Call) {
                auto call = std::static_pointer_cast<CallInst>(inst);
                if (call->getCallee() == "sph_alloc" && !call->getArgs().empty()) {
                    allocCandidates.push_back({call, bb.get()});
                }
            }
        }
    }

    if (allocCandidates.empty()) return false;

    DominatorTree domTree(function);
    auto loops = findNaturalLoops(function, domTree);

    for (const auto& [call, allocBB] : allocCandidates) {
        const auto* allocVal = call.get();
        auto allocInst = call;

        // Check if size is known and <= 512
        int64_t allocSize = 64;
        if (!call->getArgs().empty() && call->getArgs()[0]->isConstant()) {
            if (auto ci = dynamic_cast<const ConstantInt*>(call->getArgs()[0].get())) {
                allocSize = ci->getValue();
            }
        }
        if (allocSize <= 0 || allocSize > 512) continue;

        // Transitive alias analysis: collect all instructions derived from allocVal
        std::unordered_set<const IRValue*> aliases;
        std::unordered_set<const IRValue*> baseAliases;
        aliases.insert(allocVal);
        baseAliases.insert(allocVal);

        bool castEscapes = false;
        bool newAliasFound = true;
        while (newAliasFound) {
            newAliasFound = false;
            for (const auto& bb : function.getBlocks()) {
                if (!bb) continue;
                for (const auto& inst : bb->getInstructions()) {
                    if (aliases.count(inst.get())) continue;
                    auto opCode = inst->getOpcode();
                    if (opCode == IROpcode::Cast) {
                        const auto& cast = static_cast<const CastInst&>(*inst);
                        if (cast.getOperand() && aliases.count(cast.getOperand().get())) {
                            if (!isPointerLikeType(cast.getType())) {
                                castEscapes = true;
                                break;
                            }
                            aliases.insert(inst.get());
                            if (baseAliases.count(cast.getOperand().get())) {
                                baseAliases.insert(inst.get());
                            }
                            newAliasFound = true;
                        }
                    } else if (opCode == IROpcode::GetElementPtr) {
                        const auto& gep = static_cast<const GetElementPtrInst&>(*inst);
                        if (gep.getBasePtr() && aliases.count(gep.getBasePtr().get())) {
                            aliases.insert(inst.get());
                            newAliasFound = true;
                        }
                    } else if (opCode == IROpcode::GetFieldPtr) {
                        const auto& gfp = static_cast<const GetFieldPtrInst&>(*inst);
                        if (gfp.getBasePtr() && aliases.count(gfp.getBasePtr().get())) {
                            aliases.insert(inst.get());
                            newAliasFound = true;
                        }
                    } else if (opCode == IROpcode::Phi) {
                        const auto& phi = static_cast<const PhiInst&>(*inst);
                        bool hasAlias = false;
                        bool allBase = true;
                        for (const auto& [inBb, inVal] : phi.getIncoming()) {
                            if (inVal && aliases.count(inVal.get())) {
                                hasAlias = true;
                                if (!baseAliases.count(inVal.get())) {
                                    allBase = false;
                                }
                            } else {
                                allBase = false;
                            }
                        }
                        if (hasAlias) {
                            aliases.insert(inst.get());
                            if (allBase) baseAliases.insert(inst.get());
                            newAliasFound = true;
                        }
                    }
                }
                if (castEscapes) break;
            }
            if (castEscapes) break;
        }

        if (castEscapes) continue;

        // Check whether allocVal or any of its aliases escape
        bool escapes = false;
        std::vector<std::shared_ptr<IRInstruction>> freesToRemove;
        std::unordered_set<const IRInstruction*> freeSet;

        for (const auto& bb : function.getBlocks()) {
            if (!bb) continue;
            for (const auto& inst : bb->getInstructions()) {
                if (inst.get() == allocVal) continue;

                if (inst->getOpcode() == IROpcode::Ret) {
                    const auto& ret = static_cast<const RetInst&>(*inst);
                    if (ret.hasValue() && aliases.count(ret.getValue().get())) {
                        escapes = true;
                        break;
                    }
                } else if (inst->getOpcode() == IROpcode::Store) {
                    const auto& store = static_cast<const StoreInst&>(*inst);
                    if (store.getValue() && aliases.count(store.getValue().get())) {
                        escapes = true;
                        break;
                    }
                } else if (inst->getOpcode() == IROpcode::AddressOf) {
                    const auto& addr = static_cast<const AddressOfInst&>(*inst);
                    if (addr.getTarget() && aliases.count(addr.getTarget().get())) {
                        escapes = true;
                        break;
                    }
                } else if (inst->getOpcode() == IROpcode::Call) {
                    const auto& c = static_cast<const CallInst&>(*inst);
                    if (c.getCallee() == "sph_free") {
                        if (!c.getArgs().empty() && aliases.count(c.getArgs()[0].get())) {
                            if (!baseAliases.count(c.getArgs()[0].get())) {
                                escapes = true;
                                break;
                            }
                            freesToRemove.push_back(inst);
                            freeSet.insert(inst.get());
                        }
                    } else {
                        if (c.isIndirect() && c.getCalleeValue() && aliases.count(c.getCalleeValue().get())) {
                            escapes = true;
                            break;
                        }
                        for (const auto& arg : c.getArgs()) {
                            if (arg && aliases.count(arg.get())) {
                                escapes = true;
                                break;
                            }
                        }
                    }
                }

                if (escapes) break;
            }
            if (escapes) break;
        }

        if (escapes) continue;

        // Check loop containment: find innermost natural loop containing allocBB
        const NaturalLoop* innerLoop = nullptr;
        for (const auto& loop : loops) {
            if (loop.blocks.count(const_cast<IRBasicBlock*>(allocBB))) {
                if (!innerLoop || loop.blocks.size() < innerLoop->blocks.size()) {
                    innerLoop = &loop;
                }
            }
        }

        if (innerLoop) {
            if (!verifyLoopLocalLifetime(function, domTree, *innerLoop, loops, allocVal, allocBB,
                                         aliases, baseAliases, freeSet)) {
                continue;
            }
        } else if (canBasicBlockReachSelf(allocBB)) {
            // Irreducible cycle not captured by natural loops: reject conservatively
            continue;
        }

        // Demote sph_alloc to a single entry-block AllocaInst reused across iterations
        std::shared_ptr<Type> innerType = nullptr;
        if (allocInst->getType()->isBox()) {
            auto bt = std::static_pointer_cast<BoxType>(allocInst->getType());
            innerType = bt->getInnerType();
        } else if (allocInst->getType()->isPointer()) {
            auto pt = std::static_pointer_cast<PointerType>(allocInst->getType());
            innerType = pt->getPointeeType();
        }
        if (!innerType || innerType->isVoid() ||
            static_cast<int64_t>(innerType->getSize()) < allocSize) {
            innerType = ArrayType::get(Type::getU8(), static_cast<size_t>(allocSize));
        }

        auto* entryBlock = function.getBlocks().front().get();
        auto allocaInst = std::make_shared<AllocaInst>(innerType, call->getName());
        size_t insertPos = 0;
        const auto& entryInsts = entryBlock->getInstructions();
        while (insertPos < entryInsts.size() && entryInsts[insertPos]->getOpcode() == IROpcode::Alloca) {
            ++insertPos;
        }
        entryBlock->insertInstruction(insertPos, allocaInst);

        // Replace all uses of the CallInst with allocaInst
        for (auto& bb : function.getBlocks()) {
            if (!bb) continue;
            for (auto& otherInst : bb->getInstructions()) {
                if (otherInst.get() != call.get()) {
                    replaceOperandInInstruction(*otherInst, call.get(), allocaInst);
                }
            }
        }

        // Remove call and corresponding sph_free calls safely
        for (auto& bb : function.getBlocks()) {
            if (!bb) continue;
            auto& insts = bb->getInstructions();
            insts.erase(std::remove_if(insts.begin(), insts.end(), [&](const std::shared_ptr<IRInstruction>& inst) {
                if (inst == allocInst) return true;
                for (const auto& f : freesToRemove) {
                    if (inst == f) return true;
                }
                return false;
            }), insts.end());
        }

        changed = true;
    }

    return changed;
}

} // namespace sph



