#include "EscapeAnalysisPass.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/ir/IRValue.hpp"
#include <algorithm>
#include <unordered_set>
#include <vector>

namespace sph {

bool EscapeAnalysisPass::runOnFunction(IRFunction& function) {
    bool changed = false;

    std::vector<std::shared_ptr<CallInst>> allocCandidates;

    // Step 1: Find all CallInst to "sph_alloc"
    for (const auto& bb : function.getBlocks()) {
        if (!bb) continue;
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Call) {
                auto call = std::static_pointer_cast<CallInst>(inst);
                if (call->getCallee() == "sph_alloc" && !call->getArgs().empty()) {
                    allocCandidates.push_back(call);
                }
            }
        }
    }

    if (allocCandidates.empty()) return false;

    for (const auto& call : allocCandidates) {
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

        // Check whether allocVal escapes:
        // A pointer escapes if it is:
        // - Returned via RetInst
        // - Stored into memory as a value (store allocVal, otherPtr)
        // - Passed as an argument to any CallInst other than "sph_free"
        bool escapes = false;
        std::vector<std::shared_ptr<IRInstruction>> freesToRemove;

        for (size_t bbIdx = 0; bbIdx < function.getBlocks().size(); ++bbIdx) {
            const auto& bb = function.getBlocks()[bbIdx];
            for (size_t instIdx = 0; instIdx < bb->getInstructions().size(); ++instIdx) {
                const auto& inst = bb->getInstructions()[instIdx];

                if (inst.get() == allocVal) continue;

                if (inst->getOpcode() == IROpcode::Ret) {
                    const auto& ret = static_cast<const RetInst&>(*inst);
                    if (ret.hasValue() && ret.getValue().get() == allocVal) {
                        escapes = true;
                        break;
                    }
                } else if (inst->getOpcode() == IROpcode::Store) {
                    const auto& store = static_cast<const StoreInst&>(*inst);
                    if (store.getValue().get() == allocVal) {
                        escapes = true;
                        break;
                    }
                } else if (inst->getOpcode() == IROpcode::Call) {
                    const auto& c = static_cast<const CallInst&>(*inst);
                    if (c.getCallee() == "sph_free") {
                        if (!c.getArgs().empty() && c.getArgs()[0].get() == allocVal) {
                            freesToRemove.push_back(inst);
                        }
                    } else {
                        for (const auto& arg : c.getArgs()) {
                            if (arg.get() == allocVal) {
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
            // Demote sph_alloc to AllocaInst in the entry block!
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
            auto& entryInsts = entryBlock->getInstructions();
            auto insertPos = entryInsts.begin();
            while (insertPos != entryInsts.end() && (*insertPos)->getOpcode() == IROpcode::Alloca) {
                ++insertPos;
            }
            entryInsts.insert(insertPos, allocaInst);

            // Replace all uses of the CallInst with allocaInst
            for (auto& bb : function.getBlocks()) {
                for (auto& otherInst : bb->getInstructions()) {
                    if (otherInst.get() != call.get()) {
                        replaceOperandInInstruction(*otherInst, call.get(), allocaInst);
                    }
                }
            }

            // Remove call and corresponding sph_free calls safely by pointer comparison
            for (auto& bb : function.getBlocks()) {
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

