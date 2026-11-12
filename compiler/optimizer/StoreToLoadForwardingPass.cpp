#include "StoreToLoadForwardingPass.hpp"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sph {

bool StoreToLoadForwardingPass::runOnFunction(IRFunction& function) {
    bool changed = false;

    if (function.getBlocks().empty()) return false;

    IRBasicBlock* entry = function.getEntryBlock();
    if (!entry) entry = function.getBlocks().front().get();

    // Phase 1: Intra-block store-to-load forwarding and redundant load elimination
    for (auto& bb : function.getBlocks()) {
        std::unordered_map<const IRValue*, std::shared_ptr<IRValue>> currentVal;
        std::unordered_set<const IRInstruction*> loadsToRemove;

        for (auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Store) {
                auto store = std::static_pointer_cast<StoreInst>(inst);
                auto ptr = store->getPointer();
                if (dynamic_cast<const AllocaInst*>(ptr.get())) {
                    currentVal[ptr.get()] = store->getValue();
                }
            } else if (inst->getOpcode() == IROpcode::Load) {
                auto load = std::static_pointer_cast<LoadInst>(inst);
                auto ptr = load->getPointer();
                if (dynamic_cast<const AllocaInst*>(ptr.get())) {
                    auto it = currentVal.find(ptr.get());
                    if (it != currentVal.end()) {
                        auto replacement = it->second;
                        if (replacement.get() != load.get()) {
                            const IRValue* target = load.get();
                            for (auto& otherBb : function.getBlocks()) {
                                for (auto& otherInst : otherBb->getInstructions()) {
                                    if (otherInst.get() != target) {
                                        replaceOperandInInstruction(*otherInst, target, replacement);
                                    }
                                }
                            }
                            loadsToRemove.insert(load.get());
                            changed = true;
                        }
                    } else {
                        // Remember this load so subsequent loads from the same alloca in this block reuse it
                        currentVal[ptr.get()] = load;
                    }
                }
            }
        }

        if (!loadsToRemove.empty()) {
            auto& insts = bb->getInstructions();
            insts.erase(
                std::remove_if(insts.begin(), insts.end(),
                    [&](const std::shared_ptr<IRInstruction>& inst) {
                        return loadsToRemove.count(inst.get()) > 0;
                    }),
                insts.end()
            );
        }
    }

    // Phase 2: Whole-function single-store alloca forwarding
    // If an alloca has exactly one StoreInst in the entire function and that store is in the entry block,
    // every LoadInst from that alloca can be replaced with the stored value.
    std::unordered_map<const IRValue*, std::vector<std::shared_ptr<StoreInst>>> allocaStores;
    for (const auto& bb : function.getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Store) {
                auto store = std::static_pointer_cast<StoreInst>(inst);
                auto ptr = store->getPointer().get();
                if (dynamic_cast<const AllocaInst*>(ptr)) {
                    allocaStores[ptr].push_back(store);
                }
            }
        }
    }

    std::unordered_map<const IRValue*, std::shared_ptr<IRValue>> singleStoreValues;
    for (const auto& [ptr, stores] : allocaStores) {
        if (stores.size() == 1) {
            // Check if store is in the entry block
            bool inEntry = false;
            for (const auto& inst : entry->getInstructions()) {
                if (inst.get() == stores[0].get()) {
                    inEntry = true;
                    break;
                }
            }
            if (inEntry && stores[0]->getValue()) {
                singleStoreValues[ptr] = stores[0]->getValue();
            }
        }
    }

    if (!singleStoreValues.empty()) {
        for (auto& bb : function.getBlocks()) {
            std::unordered_set<const IRInstruction*> loadsToRemove;
            for (auto& inst : bb->getInstructions()) {
                if (inst->getOpcode() == IROpcode::Load) {
                    auto load = std::static_pointer_cast<LoadInst>(inst);
                    auto ptr = load->getPointer().get();
                    auto it = singleStoreValues.find(ptr);
                    if (it != singleStoreValues.end()) {
                        auto replacement = it->second;
                        if (replacement.get() != load.get()) {
                            const IRValue* target = load.get();
                            for (auto& otherBb : function.getBlocks()) {
                                for (auto& otherInst : otherBb->getInstructions()) {
                                    if (otherInst.get() != target) {
                                        replaceOperandInInstruction(*otherInst, target, replacement);
                                    }
                                }
                            }
                            loadsToRemove.insert(load.get());
                            changed = true;
                        }
                    }
                }
            }

            if (!loadsToRemove.empty()) {
                auto& insts = bb->getInstructions();
                insts.erase(
                    std::remove_if(insts.begin(), insts.end(),
                        [&](const std::shared_ptr<IRInstruction>& inst) {
                            return loadsToRemove.count(inst.get()) > 0;
                        }),
                    insts.end()
                );
            }
        }
    }

    return changed;
}

} // namespace sph

