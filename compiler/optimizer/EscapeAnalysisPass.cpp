#include "EscapeAnalysisPass.hpp"
#include "Dominance.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/ir/IRValue.hpp"
#include <algorithm>
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

} // namespace

bool EscapeAnalysisPass::runOnFunction(IRFunction& function) {
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

        // Reject if basic block is inside a loop (can reach itself) to prevent unsafe stack demotion
        if (canBasicBlockReachSelf(allocBB)) {
            continue;
        }

        // Transitive alias analysis: collect all instructions derived from allocVal
        std::unordered_set<const IRValue*> aliases;
        aliases.insert(allocVal);

        bool newAliasFound = true;
        while (newAliasFound) {
            newAliasFound = false;
            for (const auto& bb : function.getBlocks()) {
                if (!bb) continue;
                for (const auto& inst : bb->getInstructions()) {
                    if (aliases.count(inst.get())) continue;
                    if (inst->getOpcode() == IROpcode::Cast ||
                        inst->getOpcode() == IROpcode::GetElementPtr ||
                        inst->getOpcode() == IROpcode::GetFieldPtr ||
                        inst->getOpcode() == IROpcode::Phi) {
                        for (const auto* op : getInstructionOperands(*inst)) {
                            if (op && aliases.count(op)) {
                                aliases.insert(inst.get());
                                newAliasFound = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Check whether allocVal or any of its aliases escape
        bool escapes = false;
        std::vector<std::shared_ptr<IRInstruction>> freesToRemove;

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
                } else if (inst->getOpcode() == IROpcode::Call) {
                    const auto& c = static_cast<const CallInst&>(*inst);
                    if (c.getCallee() == "sph_free") {
                        if (!c.getArgs().empty() && aliases.count(c.getArgs()[0].get())) {
                            freesToRemove.push_back(inst);
                        }
                    } else {
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

        if (!escapes) {
            // Demote sph_alloc to AllocaInst in the entry block
            std::shared_ptr<Type> innerType = nullptr;
            if (allocInst->getType()->isBox()) {
                auto bt = std::static_pointer_cast<BoxType>(allocInst->getType());
                innerType = bt->getInnerType();
            } else if (allocInst->getType()->isPointer()) {
                auto pt = std::static_pointer_cast<PointerType>(allocInst->getType());
                innerType = pt->getPointeeType();
            } else {
                innerType = Type::getI64();
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
    }

    return changed;
}

} // namespace sph


