#include "FunctionInliningPass.hpp"
#include <unordered_map>

namespace sph {

bool FunctionInliningPass::isInlineCandidate(const IRFunction& fn) const noexcept {
    if (fn.getName() == "main" || fn.isDeclaration()) {
        return false;
    }
    // Only inline single-block leaf functions
    if (fn.getBlocks().size() != 1) {
        return false;
    }

    const auto& insts = fn.getBlocks().front()->getInstructions();
    if (insts.size() > m_maxInstructions) {
        return false;
    }

    // Check that it doesn't call itself or other functions, or perform complex control flow / resource cleanup
    for (const auto& inst : insts) {
        if (inst->getOpcode() == IROpcode::Call) {
            return false;
        }
        if (inst->getOpcode() == IROpcode::SetJmp) {
            return false;
        }
        if (inst->getOpcode() == IROpcode::RcRelease) {
            return false;
        }
    }

    return true;
}

bool FunctionInliningPass::inlineFunctionCall(IRFunction& caller, size_t blockIdx, size_t instIdx, const IRFunction& callee) {
    auto& callerBlock = caller.getBlocks()[blockIdx];
    auto& callerInsts = callerBlock->getInstructions();
    auto callInst = std::static_pointer_cast<CallInst>(callerInsts[instIdx]);

    const auto& calleeBlock = callee.getBlocks().front();
    const auto& calleeInsts = calleeBlock->getInstructions();

    // Map parameters to caller arguments
    std::unordered_map<const IRValue*, std::shared_ptr<IRValue>> valueMap;
    const auto& params = callee.getParameters();
    for (size_t i = 0; i < params.size() && i < callInst->getArgs().size(); ++i) {
        valueMap[params[i].get()] = callInst->getArgs()[i];
    }

    std::vector<std::shared_ptr<IRInstruction>> inlinedInsts;
    std::shared_ptr<IRValue> returnValue = nullptr;

    static size_t inlineCounter = 0;
    std::string prefix = ".inline_" + std::to_string(inlineCounter++) + "_";

    for (const auto& cInst : calleeInsts) {
        if (cInst->getOpcode() == IROpcode::Ret) {
            const auto& ret = static_cast<const RetInst&>(*cInst);
            if (ret.hasValue()) {
                auto it = valueMap.find(ret.getValue().get());
                returnValue = (it != valueMap.end()) ? it->second : ret.getValue();
            }
            break;
        }

        // Clone instruction
        std::shared_ptr<IRInstruction> cloned = nullptr;
        std::string newName = cInst->getName().empty() ? "" : (prefix + cInst->getName());

        switch (cInst->getOpcode()) {
            case IROpcode::Alloca: {
                const auto& orig = static_cast<const AllocaInst&>(*cInst);
                cloned = std::make_shared<AllocaInst>(orig.getAllocatedType(), newName);
                break;
            }
            case IROpcode::Load: {
                const auto& orig = static_cast<const LoadInst&>(*cInst);
                auto ptr = orig.getPointer();
                if (valueMap.count(ptr.get())) ptr = valueMap[ptr.get()];
                cloned = std::make_shared<LoadInst>(orig.getType(), ptr, newName);
                break;
            }
            case IROpcode::Store: {
                const auto& orig = static_cast<const StoreInst&>(*cInst);
                auto val = orig.getValue();
                auto ptr = orig.getPointer();
                if (valueMap.count(val.get())) val = valueMap[val.get()];
                if (valueMap.count(ptr.get())) ptr = valueMap[ptr.get()];
                cloned = std::make_shared<StoreInst>(val, ptr);
                break;
            }
            case IROpcode::Add:
            case IROpcode::Sub:
            case IROpcode::Mul:
            case IROpcode::Div:
            case IROpcode::Mod:
            case IROpcode::Equal:
            case IROpcode::NotEqual:
            case IROpcode::Less:
            case IROpcode::LessEqual:
            case IROpcode::Greater:
            case IROpcode::GreaterEqual:
            case IROpcode::LogicalAnd:
            case IROpcode::LogicalOr:
            case IROpcode::BitwiseAnd:
            case IROpcode::BitwiseOr:
            case IROpcode::BitwiseXor:
            case IROpcode::ShiftLeft:
            case IROpcode::ShiftRight: {
                const auto& orig = static_cast<const BinaryInst&>(*cInst);
                auto lhs = orig.getLhs();
                auto rhs = orig.getRhs();
                if (valueMap.count(lhs.get())) lhs = valueMap[lhs.get()];
                if (valueMap.count(rhs.get())) rhs = valueMap[rhs.get()];
                cloned = std::make_shared<BinaryInst>(orig.getOpcode(), lhs, rhs, orig.getType(), newName);
                break;
            }
            case IROpcode::Negate:
            case IROpcode::LogicalNot:
            case IROpcode::BitwiseNot: {
                const auto& orig = static_cast<const UnaryInst&>(*cInst);
                auto op = orig.getOperand();
                if (valueMap.count(op.get())) op = valueMap[op.get()];
                cloned = std::make_shared<UnaryInst>(orig.getOpcode(), op, orig.getType(), newName);
                break;
            }
            case IROpcode::Cast: {
                const auto& orig = static_cast<const CastInst&>(*cInst);
                auto op = orig.getOperand();
                if (valueMap.count(op.get())) op = valueMap[op.get()];
                cloned = std::make_shared<CastInst>(op, orig.getType(), newName);
                break;
            }
            case IROpcode::GetFieldPtr: {
                const auto& orig = static_cast<const GetFieldPtrInst&>(*cInst);
                auto base = orig.getBasePtr();
                if (valueMap.count(base.get())) base = valueMap[base.get()];
                cloned = std::make_shared<GetFieldPtrInst>(base, orig.getFieldIndex(), orig.getByteOffset(), orig.getType(), newName);
                break;
            }
            case IROpcode::GetElementPtr: {
                const auto& orig = static_cast<const GetElementPtrInst&>(*cInst);
                auto base = orig.getBasePtr();
                auto idx = orig.getIndex();
                if (valueMap.count(base.get())) base = valueMap[base.get()];
                if (valueMap.count(idx.get())) idx = valueMap[idx.get()];
                cloned = std::make_shared<GetElementPtrInst>(base, idx, orig.getElementSize(), orig.getType(), newName);
                break;
            }
            case IROpcode::AddressOf: {
                const auto& orig = static_cast<const AddressOfInst&>(*cInst);
                auto target = orig.getTarget();
                if (valueMap.count(target.get())) target = valueMap[target.get()];
                cloned = std::make_shared<AddressOfInst>(target, orig.getType(), newName);
                break;
            }
            default:
                break;
        }

        if (cloned) {
            valueMap[cInst.get()] = cloned;
            inlinedInsts.push_back(cloned);
        }
    }

    // Replace uses of callInst in caller with returnValue
    if (returnValue && !callInst->getType()->isVoid()) {
        const IRValue* target = callInst.get();
        for (auto& b : caller.getBlocks()) {
            for (auto& inst : b->getInstructions()) {
                if (inst.get() != target) {
                    replaceOperandInInstruction(*inst, target, returnValue);
                }
            }
        }
    }

    // Replace callInst with inlined instructions
    callerInsts.erase(callerInsts.begin() + instIdx);
    callerInsts.insert(callerInsts.begin() + instIdx, inlinedInsts.begin(), inlinedInsts.end());

    return true;
}

bool FunctionInliningPass::runOnModule(IRModule& module) {
    bool anyChanged = false;

    // Collect inline candidates
    std::unordered_map<std::string, const IRFunction*> candidateMap;
    for (const auto& fn : module.getFunctions()) {
        if (isInlineCandidate(*fn)) {
            candidateMap[fn->getName()] = fn.get();
        }
    }

    if (candidateMap.empty()) return false;

    // Search for calls to candidates in other functions
    for (auto& fn : module.getFunctions()) {
        if (candidateMap.count(fn->getName())) continue; // Don't inline inside another candidate

        bool fnChanged = true;
        while (fnChanged) {
            fnChanged = false;
            for (size_t bIdx = 0; bIdx < fn->getBlocks().size(); ++bIdx) {
                auto& insts = fn->getBlocks()[bIdx]->getInstructions();
                for (size_t i = 0; i < insts.size(); ++i) {
                    if (insts[i]->getOpcode() == IROpcode::Call) {
                        auto call = std::static_pointer_cast<CallInst>(insts[i]);
                        if (!call->isIndirect()) {
                            auto it = candidateMap.find(call->getCallee());
                            if (it != candidateMap.end()) {
                                inlineFunctionCall(*fn, bIdx, i, *it->second);
                                fnChanged = true;
                                anyChanged = true;
                                break;
                            }
                        }
                    }
                }
                if (fnChanged) break;
            }
        }
    }

    return anyChanged;
}

} // namespace sph

