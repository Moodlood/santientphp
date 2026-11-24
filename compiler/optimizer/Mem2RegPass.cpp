#include "Mem2RegPass.hpp"
#include "Dominance.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/ir/IRValue.hpp"
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sph {

namespace {

[[maybe_unused]] std::shared_ptr<IRValue> createDefaultZero(const std::shared_ptr<Type>& type) {
    if (type->isBoolean()) {
        return ConstantBool::get(false);
    }
    if (type->isFloatingPoint()) {
        return ConstantFloat::get(0.0, type);
    }
    return ConstantInt::get(0, type);
}

} // namespace

bool Mem2RegPass::runOnFunction(IRFunction& function) {
    if (function.getBlocks().empty()) return false;

    // Phase 1: Collect non-escaped AllocaInst candidates
    // An alloca is non-escaped if it is ONLY used directly as:
    // 1. StoreInst pointer (store val, alloca)
    // 2. LoadInst pointer (load alloca)
    std::unordered_set<const IRValue*> escapedAllocas;
    std::vector<AllocaInst*> candidates;

    for (const auto& bb : function.getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Load) {
                continue;
            }
            if (inst->getOpcode() == IROpcode::Store) {
                auto* store = static_cast<const StoreInst*>(inst.get());
                if (dynamic_cast<const AllocaInst*>(store->getValue().get())) {
                    escapedAllocas.insert(store->getValue().get());
                }
                continue;
            }
            auto operands = getInstructionOperands(*inst);
            for (const auto* op : operands) {
                if (dynamic_cast<const AllocaInst*>(op)) {
                    escapedAllocas.insert(op);
                }
            }
        }
    }

    IRBasicBlock* entry = function.getEntryBlock();
    if (!entry) entry = function.getBlocks().front().get();

    for (const auto& inst : entry->getInstructions()) {
        if (inst->getOpcode() == IROpcode::Alloca) {
            auto* alloca = static_cast<AllocaInst*>(inst.get());
            if (!escapedAllocas.count(alloca)) {
                // Must be scalar or pointer (not big struct/array)
                auto allocType = alloca->getAllocatedType();
                if (allocType->isInteger() || allocType->isFloatingPoint() ||
                    allocType->isBoolean() || allocType->isPointer() ||
                    allocType->isBox()) {
                    candidates.push_back(alloca);
                }
            }
        }
    }

    if (candidates.empty()) return false;

    // Phase 2: Compute Dominator Tree and place Phi nodes
    DominatorTree domTree(function);

    // Track placed Phi nodes: map from Alloca to map of Block -> PhiInst
    std::unordered_map<const AllocaInst*, std::unordered_map<IRBasicBlock*, std::shared_ptr<PhiInst>>> placedPhis;

    for (auto* alloca : candidates) {
        std::unordered_set<IRBasicBlock*> defBlocks;
        for (const auto& bb : function.getBlocks()) {
            for (const auto& inst : bb->getInstructions()) {
                if (inst->getOpcode() == IROpcode::Store) {
                    auto* store = static_cast<const StoreInst*>(inst.get());
                    if (store->getPointer().get() == alloca) {
                        defBlocks.insert(bb.get());
                    }
                }
            }
        }

        auto idf = domTree.computeIDF(defBlocks);
        for (auto* bb : idf) {
            auto phi = std::make_shared<PhiInst>(
                alloca->getAllocatedType(),
                alloca->getName().empty() ? "phi" : (alloca->getName() + ".phi"));
            placedPhis[alloca][bb] = phi;

            // Insert phi at the beginning of the block (before non-phi instructions)
            const auto& insts = bb->getInstructions();
            size_t insertIdx = 0;
            while (insertIdx < insts.size() && insts[insertIdx]->getOpcode() == IROpcode::Phi) {
                ++insertIdx;
            }
            bb->insertInstruction(insertIdx, phi);
        }
    }

    // Phase 3: Variable Renaming via Dominator Tree Preorder Traversal
    std::unordered_map<const AllocaInst*, std::vector<std::shared_ptr<IRValue>>> defStacks;
    std::unordered_set<const IRInstruction*> toRemove;

    std::function<void(IRBasicBlock*)> renameBlock = [&](IRBasicBlock* bb) {
        // Track how many values were pushed to each alloca stack in this block
        std::unordered_map<const AllocaInst*, size_t> pushedCounts;

        auto& insts = bb->getInstructions();
        for (auto& inst : insts) {
            if (inst->getOpcode() == IROpcode::Phi) {
                auto phi = std::static_pointer_cast<PhiInst>(inst);
                // Check which alloca this phi corresponds to
                for (auto* alloca : candidates) {
                    auto it = placedPhis.find(alloca);
                    if (it != placedPhis.end()) {
                        auto bIt = it->second.find(bb);
                        if (bIt != it->second.end() && bIt->second.get() == phi.get()) {
                            defStacks[alloca].push_back(phi);
                            pushedCounts[alloca]++;
                            break;
                        }
                    }
                }
            } else if (inst->getOpcode() == IROpcode::Store) {
                auto store = std::static_pointer_cast<StoreInst>(inst);
                for (auto* alloca : candidates) {
                    if (store->getPointer().get() == alloca) {
                        defStacks[alloca].push_back(store->getValue());
                        pushedCounts[alloca]++;
                        toRemove.insert(store.get());
                        break;
                    }
                }
            } else if (inst->getOpcode() == IROpcode::Load) {
                auto load = std::static_pointer_cast<LoadInst>(inst);
                for (auto* alloca : candidates) {
                    if (load->getPointer().get() == alloca) {
                        std::shared_ptr<IRValue> val = nullptr;
                        if (!defStacks[alloca].empty()) {
                            val = defStacks[alloca].back();
                        } else {
                            val = createDefaultZero(alloca->getAllocatedType());
                        }

                        // Replace all uses of this load across all instructions
                        for (auto& otherBb : function.getBlocks()) {
                            for (auto& otherInst : otherBb->getInstructions()) {
                                if (otherInst.get() != load.get()) {
                                    replaceOperandInInstruction(*otherInst, load.get(), val);
                                }
                            }
                        }
                        toRemove.insert(load.get());
                        break;
                    }
                }
            }
        }

        // Fill incoming values for Phis in all CFG successors
        auto succs = getSuccessors(*bb);
        for (auto* succ : succs) {
            for (auto* alloca : candidates) {
                auto it = placedPhis.find(alloca);
                if (it != placedPhis.end()) {
                    auto pIt = it->second.find(succ);
                    if (pIt != it->second.end()) {
                        auto phi = pIt->second;
                        std::shared_ptr<IRValue> val = nullptr;
                        if (!defStacks[alloca].empty()) {
                            val = defStacks[alloca].back();
                        } else {
                            val = createDefaultZero(alloca->getAllocatedType());
                        }
                        phi->addIncoming(bb, val);
                    }
                }
            }
        }

        // Recurse to Dominator Tree children
        for (auto* child : domTree.getChildren(bb)) {
            renameBlock(child);
        }

        // Unwind definition stacks
        for (const auto& [alloca, count] : pushedCounts) {
            for (size_t i = 0; i < count; ++i) {
                if (!defStacks[alloca].empty()) {
                    defStacks[alloca].pop_back();
                }
            }
        }
    };

    renameBlock(entry);

    // Phase 4: Erase promoted allocas and marked instructions
    for (auto* alloca : candidates) {
        toRemove.insert(alloca);
    }

    for (auto& bb : function.getBlocks()) {
        auto& insts = bb->getInstructions();
        insts.erase(
            std::remove_if(insts.begin(), insts.end(),
                [&toRemove](const std::shared_ptr<IRInstruction>& inst) {
                    return toRemove.count(inst.get()) > 0;
                }),
            insts.end());
    }

    // Phase 5: Prune trivial / redundant Phis
    bool simplified = true;
    while (simplified) {
        simplified = false;
        for (auto& bb : function.getBlocks()) {
            auto& insts = bb->getInstructions();
            for (auto it = insts.begin(); it != insts.end(); ) {
                if ((*it)->getOpcode() == IROpcode::Phi) {
                    auto phi = std::static_pointer_cast<PhiInst>(*it);
                    std::shared_ptr<IRValue> singleVal = nullptr;
                    bool redundant = true;

                    for (const auto& [inBb, inVal] : phi->getIncoming()) {
                        if (!inVal || inVal.get() == phi.get()) continue;
                        if (!singleVal) {
                            singleVal = inVal;
                        } else if (singleVal.get() != inVal.get()) {
                            redundant = false;
                            break;
                        }
                    }

                    if (redundant && singleVal) {
                        for (auto& otherBb : function.getBlocks()) {
                            for (auto& otherInst : otherBb->getInstructions()) {
                                if (otherInst.get() != phi.get()) {
                                    replaceOperandInInstruction(*otherInst, phi.get(), singleVal);
                                }
                            }
                        }
                        it = insts.erase(it);
                        simplified = true;
                        continue;
                    }
                }
                ++it;
            }
        }
    }

    return true;
}

} // namespace sph

