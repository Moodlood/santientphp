#include "IRInstruction.hpp"

namespace sph {

const char* opcodeToString(IROpcode op) noexcept {
    switch (op) {
        case IROpcode::Alloca:       return "alloca";
        case IROpcode::Load:         return "load";
        case IROpcode::Store:        return "store";
        case IROpcode::GetFieldPtr:   return "getfieldptr";
        case IROpcode::GetElementPtr: return "getelementptr";
        case IROpcode::AddressOf:    return "addressof";
        case IROpcode::Add:          return "add";
        case IROpcode::Sub:          return "sub";
        case IROpcode::Mul:          return "mul";
        case IROpcode::Div:          return "div";
        case IROpcode::Mod:          return "mod";
        case IROpcode::Equal:        return "eq";
        case IROpcode::NotEqual:     return "ne";
        case IROpcode::Less:         return "lt";
        case IROpcode::LessEqual:    return "le";
        case IROpcode::Greater:      return "gt";
        case IROpcode::GreaterEqual: return "ge";
        case IROpcode::LogicalAnd:   return "and";
        case IROpcode::LogicalOr:    return "or";
        case IROpcode::BitwiseAnd:   return "bit_and";
        case IROpcode::BitwiseOr:    return "bit_or";
        case IROpcode::BitwiseXor:   return "bit_xor";
        case IROpcode::ShiftLeft:    return "shl";
        case IROpcode::ShiftRight:   return "shr";
        case IROpcode::Negate:       return "neg";
        case IROpcode::LogicalNot:   return "not";
        case IROpcode::BitwiseNot:   return "bit_not";
        case IROpcode::Cast:         return "cast";
        case IROpcode::Ret:          return "ret";
        case IROpcode::Br:           return "br";
        case IROpcode::CondBr:       return "cond_br";
        case IROpcode::Call:         return "call";
        case IROpcode::RcAlloc:      return "rc_alloc";
        case IROpcode::RcRetain:     return "rc_retain";
        case IROpcode::RcRelease:    return "rc_release";
        case IROpcode::SetJmp:       return "setjmp";
    }
    return "unknown";
}

bool replaceOperandInInstruction(IRInstruction& inst, const IRValue* oldVal, std::shared_ptr<IRValue> newVal) {
    if (!oldVal || !newVal) return false;
    bool replaced = false;

    switch (inst.getOpcode()) {
        case IROpcode::Load: {
            auto& load = static_cast<LoadInst&>(inst);
            if (load.getPointer().get() == oldVal) {
                load.setPointer(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::AddressOf: {
            auto& addr = static_cast<AddressOfInst&>(inst);
            if (addr.getTarget().get() == oldVal) {
                addr.setTarget(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::Store: {
            auto& store = static_cast<StoreInst&>(inst);
            if (store.getValue().get() == oldVal) {
                store.setValue(newVal);
                replaced = true;
            }
            if (store.getPointer().get() == oldVal) {
                store.setPointer(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::GetFieldPtr: {
            auto& gfp = static_cast<GetFieldPtrInst&>(inst);
            if (gfp.getBasePtr().get() == oldVal) {
                gfp.setBasePtr(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::GetElementPtr: {
            auto& gep = static_cast<GetElementPtrInst&>(inst);
            if (gep.getBasePtr().get() == oldVal) {
                gep.setBasePtr(newVal);
                replaced = true;
            }
            if (gep.getIndex().get() == oldVal) {
                gep.setIndex(newVal);
                replaced = true;
            }
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
            auto& bin = static_cast<BinaryInst&>(inst);
            if (bin.getLhs().get() == oldVal) {
                bin.setLhs(newVal);
                replaced = true;
            }
            if (bin.getRhs().get() == oldVal) {
                bin.setRhs(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::Negate:
        case IROpcode::LogicalNot:
        case IROpcode::BitwiseNot: {
            auto& un = static_cast<UnaryInst&>(inst);
            if (un.getOperand().get() == oldVal) {
                un.setOperand(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::Cast: {
            auto& cast = static_cast<CastInst&>(inst);
            if (cast.getOperand().get() == oldVal) {
                cast.setOperand(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::Ret: {
            auto& ret = static_cast<RetInst&>(inst);
            if (ret.hasValue() && ret.getValue().get() == oldVal) {
                ret.setValue(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::CondBr: {
            auto& condBr = static_cast<CondBranchInst&>(inst);
            if (condBr.getCondition().get() == oldVal) {
                condBr.setCondition(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::Call: {
            auto& call = static_cast<CallInst&>(inst);
            if (call.isIndirect() && call.getCalleeValue().get() == oldVal) {
                call.setCalleeValue(newVal);
                replaced = true;
            }
            for (size_t i = 0; i < call.getArgs().size(); ++i) {
                if (call.getArgs()[i].get() == oldVal) {
                    call.setArg(i, newVal);
                    replaced = true;
                }
            }
            break;
        }
        case IROpcode::RcRetain: {
            auto& rc = static_cast<RcRetainInst&>(inst);
            if (rc.getPtr().get() == oldVal) {
                rc.setPtr(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::RcRelease: {
            auto& rc = static_cast<RcReleaseInst&>(inst);
            if (rc.getPtr().get() == oldVal) {
                rc.setPtr(newVal);
                replaced = true;
            }
            break;
        }
        case IROpcode::SetJmp: {
            auto& sj = static_cast<SetJmpInst&>(inst);
            if (sj.getFramePtr().get() == oldVal) {
                sj.setFramePtr(newVal);
                replaced = true;
            }
            break;
        }
        default:
            break;
    }

    return replaced;
}

std::vector<const IRValue*> getInstructionOperands(const IRInstruction& inst) {
    std::vector<const IRValue*> operands;

    switch (inst.getOpcode()) {
        case IROpcode::Load: {
            const auto& load = static_cast<const LoadInst&>(inst);
            if (load.getPointer()) operands.push_back(load.getPointer().get());
            break;
        }
        case IROpcode::AddressOf: {
            const auto& addr = static_cast<const AddressOfInst&>(inst);
            if (addr.getTarget()) operands.push_back(addr.getTarget().get());
            break;
        }
        case IROpcode::Store: {
            const auto& store = static_cast<const StoreInst&>(inst);
            if (store.getValue()) operands.push_back(store.getValue().get());
            if (store.getPointer()) operands.push_back(store.getPointer().get());
            break;
        }
        case IROpcode::GetFieldPtr: {
            const auto& gfp = static_cast<const GetFieldPtrInst&>(inst);
            if (gfp.getBasePtr()) operands.push_back(gfp.getBasePtr().get());
            break;
        }
        case IROpcode::GetElementPtr: {
            const auto& gep = static_cast<const GetElementPtrInst&>(inst);
            if (gep.getBasePtr()) operands.push_back(gep.getBasePtr().get());
            if (gep.getIndex()) operands.push_back(gep.getIndex().get());
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
            const auto& bin = static_cast<const BinaryInst&>(inst);
            if (bin.getLhs()) operands.push_back(bin.getLhs().get());
            if (bin.getRhs()) operands.push_back(bin.getRhs().get());
            break;
        }
        case IROpcode::Negate:
        case IROpcode::LogicalNot:
        case IROpcode::BitwiseNot: {
            const auto& un = static_cast<const UnaryInst&>(inst);
            if (un.getOperand()) operands.push_back(un.getOperand().get());
            break;
        }
        case IROpcode::Cast: {
            const auto& cast = static_cast<const CastInst&>(inst);
            if (cast.getOperand()) operands.push_back(cast.getOperand().get());
            break;
        }
        case IROpcode::Ret: {
            const auto& ret = static_cast<const RetInst&>(inst);
            if (ret.hasValue() && ret.getValue()) operands.push_back(ret.getValue().get());
            break;
        }
        case IROpcode::CondBr: {
            const auto& condBr = static_cast<const CondBranchInst&>(inst);
            if (condBr.getCondition()) operands.push_back(condBr.getCondition().get());
            break;
        }
        case IROpcode::Call: {
            const auto& call = static_cast<const CallInst&>(inst);
            if (call.isIndirect() && call.getCalleeValue()) {
                operands.push_back(call.getCalleeValue().get());
            }
            for (const auto& arg : call.getArgs()) {
                if (arg) operands.push_back(arg.get());
            }
            break;
        }
        case IROpcode::RcRetain: {
            const auto& rc = static_cast<const RcRetainInst&>(inst);
            if (rc.getPtr()) operands.push_back(rc.getPtr().get());
            break;
        }
        case IROpcode::RcRelease: {
            const auto& rc = static_cast<const RcReleaseInst&>(inst);
            if (rc.getPtr()) operands.push_back(rc.getPtr().get());
            break;
        }
        case IROpcode::SetJmp: {
            const auto& sj = static_cast<const SetJmpInst&>(inst);
            if (sj.getFramePtr()) operands.push_back(sj.getFramePtr().get());
            break;
        }
        default:
            break;
    }

    return operands;
}

} // namespace sph

