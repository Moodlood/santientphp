#include "DeadStoreEliminationPass.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sph {

bool DeadStoreEliminationPass::runOnFunction(IRFunction& fn) {
    bool changed = false;

    // Phase 1: Identify local Allocas that are never read and never escape
    std::unordered_set<const IRValue*> allAllocas;
    std::unordered_set<const IRValue*> readOrEscapedAllocas;

    for (const auto& bb : fn.getBlocks()) {
        if (!bb) continue;
        for (const auto& inst : bb->getInstructions()) {
            if (!inst) continue;
            if (inst->getOpcode() == IROpcode::Alloca) {
                allAllocas.insert(inst.get());
            }
            if (auto* load = dynamic_cast<const LoadInst*>(inst.get())) {
                if (load->getPointer()) {
                    readOrEscapedAllocas.insert(load->getPointer().get());
                    const IRValue* base = load->getPointer().get();
                    while (base && base->isInstruction()) {
                        auto op = static_cast<const IRInstruction*>(base)->getOpcode();
                        if (op == IROpcode::GetFieldPtr) {
                            base = static_cast<const GetFieldPtrInst*>(base)->getBasePtr().get();
                            if (base) readOrEscapedAllocas.insert(base);
                        } else if (op == IROpcode::GetElementPtr) {
                            base = static_cast<const GetElementPtrInst*>(base)->getBasePtr().get();
                            if (base) readOrEscapedAllocas.insert(base);
                        } else if (op == IROpcode::AddressOf) {
                            base = static_cast<const AddressOfInst*>(base)->getTarget().get();
                            if (base) readOrEscapedAllocas.insert(base);
                        } else {
                            break;
                        }
                    }
                }
            }
            if (auto* gfp = dynamic_cast<const GetFieldPtrInst*>(inst.get())) {
                if (gfp->getBasePtr()) readOrEscapedAllocas.insert(gfp->getBasePtr().get());
            }
            if (auto* gep = dynamic_cast<const GetElementPtrInst*>(inst.get())) {
                if (gep->getBasePtr()) readOrEscapedAllocas.insert(gep->getBasePtr().get());
            }
            if (auto* addrOf = dynamic_cast<const AddressOfInst*>(inst.get())) {
                if (addrOf->getTarget()) readOrEscapedAllocas.insert(addrOf->getTarget().get());
            }
            if (auto* call = dynamic_cast<const CallInst*>(inst.get())) {
                for (const auto& arg : call->getArgs()) {
                    if (arg) {
                        readOrEscapedAllocas.insert(arg.get());
                        const IRValue* base = arg.get();
                        while (base && base->isInstruction()) {
                            auto op = static_cast<const IRInstruction*>(base)->getOpcode();
                            if (op == IROpcode::GetFieldPtr) {
                                base = static_cast<const GetFieldPtrInst*>(base)->getBasePtr().get();
                                if (base) readOrEscapedAllocas.insert(base);
                            } else if (op == IROpcode::GetElementPtr) {
                                base = static_cast<const GetElementPtrInst*>(base)->getBasePtr().get();
                                if (base) readOrEscapedAllocas.insert(base);
                            } else if (op == IROpcode::AddressOf) {
                                base = static_cast<const AddressOfInst*>(base)->getTarget().get();
                                if (base) readOrEscapedAllocas.insert(base);
                            } else {
                                break;
                            }
                        }
                    }
                }
            }
            if (auto* ret = dynamic_cast<const RetInst*>(inst.get())) {
                if (ret->hasValue() && ret->getValue()) {
                    readOrEscapedAllocas.insert(ret->getValue().get());
                }
            }
            if (auto* store = dynamic_cast<const StoreInst*>(inst.get())) {
                if (store->getValue()) readOrEscapedAllocas.insert(store->getValue().get());
            }
        }
    }

    std::unordered_set<const IRValue*> unreadAllocas;
    for (const auto* alc : allAllocas) {
        if (!readOrEscapedAllocas.count(alc)) {
            unreadAllocas.insert(alc);
        }
    }

    // Phase 2: Eliminate dead stores across blocks
    for (auto& bb : fn.getBlocks()) {
        if (!bb) continue;
        auto& insts = bb->getInstructions();
        std::unordered_set<size_t> deadStoreIndices;
        std::unordered_map<const IRValue*, size_t> activeStoreToPtr;

        for (size_t i = 0; i < insts.size(); ++i) {
            auto* inst = insts[i].get();
            if (!inst) continue;

            if (auto* store = dynamic_cast<StoreInst*>(inst)) {
                const auto* ptr = store->getPointer() ? store->getPointer().get() : nullptr;
                if (!ptr) continue;

                // If pointer is an unread local alloca, store is dead
                const IRValue* base = ptr;
                while (base && base->isInstruction()) {
                    auto op = static_cast<const IRInstruction*>(base)->getOpcode();
                    if (op == IROpcode::GetFieldPtr) {
                        base = static_cast<const GetFieldPtrInst*>(base)->getBasePtr().get();
                    } else if (op == IROpcode::GetElementPtr) {
                        base = static_cast<const GetElementPtrInst*>(base)->getBasePtr().get();
                    } else if (op == IROpcode::AddressOf) {
                        base = static_cast<const AddressOfInst*>(base)->getTarget().get();
                    } else {
                        break;
                    }
                }
                if (unreadAllocas.count(ptr) || (base && unreadAllocas.count(base))) {
                    deadStoreIndices.insert(i);
                    continue;
                }

                // If another store to same pointer happened without intervening read/call
                auto it = activeStoreToPtr.find(ptr);
                if (it != activeStoreToPtr.end()) {
                    deadStoreIndices.insert(it->second);
                }
                activeStoreToPtr[ptr] = i;
            } else if (auto* load = dynamic_cast<LoadInst*>(inst)) {
                if (load->getPointer()) {
                    activeStoreToPtr.erase(load->getPointer().get());
                }
            } else if (inst->getOpcode() == IROpcode::Call) {
                activeStoreToPtr.clear();
            }
        }

        if (!deadStoreIndices.empty()) {
            std::vector<std::shared_ptr<IRInstruction>> remaining;
            remaining.reserve(insts.size() - deadStoreIndices.size());
            for (size_t i = 0; i < insts.size(); ++i) {
                if (!deadStoreIndices.count(i)) {
                    remaining.push_back(std::move(insts[i]));
                }
            }
            insts = std::move(remaining);
            changed = true;
        }
    }

    return changed;
}

} // namespace sph
