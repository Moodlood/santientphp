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

    // Escape analysis for allocas:
    // An alloca is non-escaped if and only if it is ONLY used directly as:
    // 1. StoreInst pointer (store val, alloca)
    // 2. LoadInst pointer (load alloca)
    // If an alloca is used as an operand in ANY other instruction
    // (CallInst, AddressOfInst, GetFieldPtrInst, GetElementPtrInst, CastInst, StoreInst value, RetInst, etc.),
    // then its address escapes and it can be aliased or mutated indirectly.
    std::unordered_set<const IRValue*> escapedAllocas;
    for (const auto& bb : function.getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Load) {
                // Direct load from alloca does not escape the alloca's address
                continue;
            }
            if (inst->getOpcode() == IROpcode::Store) {
                auto store = static_cast<const StoreInst*>(inst.get());
                // Storing the alloca itself as a value into memory escapes it
                if (dynamic_cast<const AllocaInst*>(store->getValue().get())) {
                    escapedAllocas.insert(store->getValue().get());
                }
                continue;
            }
            // For any other instruction, if an alloca is used as an operand, it escapes
            auto operands = getInstructionOperands(*inst);
            for (const auto* op : operands) {
                if (dynamic_cast<const AllocaInst*>(op)) {
                    escapedAllocas.insert(op);
                }
            }
        }
    }

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
                } else {
                    // Indirect store through a non-alloca pointer (e.g. raw pointer, GEP)
                    // This store could alias any escaped alloca, so invalidate all escaped allocas.
                    for (auto it = currentVal.begin(); it != currentVal.end(); ) {
                        if (escapedAllocas.count(it->first)) {
                            it = currentVal.erase(it);
                        } else {
                            ++it;
                        }
                    }
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
            } else if (inst->getOpcode() == IROpcode::Call || inst->getOpcode() == IROpcode::RcRelease) {
                // Function calls and RcRelease can invoke arbitrary code or callbacks that read/write
                // any escaped memory or global state. Invalidate all escaped allocas!
                for (auto it = currentVal.begin(); it != currentVal.end(); ) {
                    if (escapedAllocas.count(it->first)) {
                        it = currentVal.erase(it);
                    } else {
                        ++it;
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
    // If an alloca has exactly one StoreInst in the entire function, that store is in the entry block,
    // AND the alloca never escapes (!escapedAllocas.count(ptr)),
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
        if (stores.size() == 1 && escapedAllocas.count(ptr) == 0) {
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
