#include "IRVerifier.hpp"

namespace sph {

bool IRVerifier::verify(const IRModule& module, std::vector<std::string>& errors) const {
    bool ok = true;
    for (const auto& fn : module.getFunctions()) {
        if (!verify(*fn, errors)) {
            ok = false;
        }
    }
    return ok;
}

bool IRVerifier::verify(const IRFunction& fn, std::vector<std::string>& errors) const {
    if (fn.isDeclaration()) {
        return true;
    }

    bool ok = true;

    if (fn.getBlocks().empty()) {
        errors.push_back("function '@" + fn.getName() + "' has no basic blocks");
        return false;
    }

    for (const auto& bb : fn.getBlocks()) {
        if (!verify(*bb, fn, errors)) {
            ok = false;
        }
    }

    return ok;
}

bool IRVerifier::verify(const IRBasicBlock& bb, const IRFunction& fn, std::vector<std::string>& errors) const {
    bool ok = true;
    const auto& instructions = bb.getInstructions();

    if (instructions.empty()) {
        errors.push_back("basic block '" + bb.getName() + "' in function '@" + fn.getName() + "' is empty");
        return false;
    }

    if (!bb.hasTerminator()) {
        errors.push_back("basic block '" + bb.getName() + "' in function '@" + fn.getName() +
                         "' does not end with a terminator");
        ok = false;
    }

    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& inst = instructions[i];

        // Ensure terminator only at the end
        if (inst->isTerminator() && i != instructions.size() - 1) {
            errors.push_back("terminator instruction appears before the end of block '" + bb.getName() + "'");
            ok = false;
        }

        // Validate specific instructions
        switch (inst->getOpcode()) {
            case IROpcode::Ret: {
                const auto& retInst = static_cast<const RetInst&>(*inst);
                if (fn.getReturnType()->isVoid()) {
                    if (retInst.hasValue()) {
                        errors.push_back("void function '@" + fn.getName() + "' returns a value");
                        ok = false;
                    }
                } else {
                    if (!retInst.hasValue()) {
                        errors.push_back("non-void function '@" + fn.getName() + "' returns void");
                        ok = false;
                    } else if (!retInst.getValue()->getType()->isEqual(*fn.getReturnType())) {
                        errors.push_back("return type mismatch in function '@" + fn.getName() +
                                         "': expected '" + fn.getReturnType()->toString() +
                                         "', got '" + retInst.getValue()->getType()->toString() + "'");
                        ok = false;
                    }
                }
                break;
            }
            case IROpcode::Br: {
                const auto& brInst = static_cast<const BranchInst&>(*inst);
                if (!brInst.getTarget()) {
                    errors.push_back("branch instruction in block '" + bb.getName() + "' has null target");
                    ok = false;
                } else if (brInst.getTarget()->getParent() != &fn) {
                    errors.push_back("branch target does not belong to enclosing function '@" + fn.getName() + "'");
                    ok = false;
                }
                break;
            }
            case IROpcode::CondBr: {
                const auto& condBr = static_cast<const CondBranchInst&>(*inst);
                if (!condBr.getCondition()) {
                    errors.push_back("conditional branch in block '" + bb.getName() + "' has null condition");
                    ok = false;
                } else if (!condBr.getCondition()->getType()->isBoolean()) {
                    errors.push_back("conditional branch condition must be bool, found '" +
                                     condBr.getCondition()->getType()->toString() + "'");
                    ok = false;
                }

                if (!condBr.getTrueTarget() || condBr.getTrueTarget()->getParent() != &fn) {
                    errors.push_back("conditional branch true target invalid in function '@" + fn.getName() + "'");
                    ok = false;
                }
                if (!condBr.getFalseTarget() || condBr.getFalseTarget()->getParent() != &fn) {
                    errors.push_back("conditional branch false target invalid in function '@" + fn.getName() + "'");
                    ok = false;
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
                const auto& binInst = static_cast<const BinaryInst&>(*inst);
                if (!binInst.getLhs() || !binInst.getRhs()) {
                    errors.push_back("binary instruction has null operand");
                    ok = false;
                }
                break;
            }
            case IROpcode::Negate:
            case IROpcode::LogicalNot:
            case IROpcode::BitwiseNot: {
                const auto& unInst = static_cast<const UnaryInst&>(*inst);
                if (!unInst.getOperand()) {
                    errors.push_back("unary instruction has null operand");
                    ok = false;
                }
                break;
            }
            case IROpcode::Cast: {
                const auto& castInst = static_cast<const CastInst&>(*inst);
                if (!castInst.getOperand()) {
                    errors.push_back("cast instruction has null operand");
                    ok = false;
                }
                break;
            }
            case IROpcode::GetFieldPtr: {
                const auto& gfp = static_cast<const GetFieldPtrInst&>(*inst);
                if (!gfp.getBasePtr()) {
                    errors.push_back("getfieldptr instruction has null base pointer");
                    ok = false;
                }
                break;
            }
            case IROpcode::GetElementPtr: {
                const auto& gep = static_cast<const GetElementPtrInst&>(*inst);
                if (!gep.getBasePtr()) {
                    errors.push_back("getelementptr instruction has null base pointer");
                    ok = false;
                }
                if (!gep.getIndex()) {
                    errors.push_back("getelementptr instruction has null index");
                    ok = false;
                }
                break;
            }
            case IROpcode::AddressOf: {
                const auto& addrInst = static_cast<const AddressOfInst&>(*inst);
                if (!addrInst.getTarget()) {
                    errors.push_back("addressof instruction has null target");
                    ok = false;
                } else if (!addrInst.getType()->isPointer()) {
                    errors.push_back("addressof instruction result type must be pointer");
                    ok = false;
                }
                break;
            }
            case IROpcode::RcAlloc: {
                const auto& rcAlloc = static_cast<const RcAllocInst&>(*inst);
                if (!rcAlloc.getType()->isRc()) {
                    errors.push_back("rc_alloc instruction result type must be Rc<T>");
                    ok = false;
                }
                break;
            }
            case IROpcode::RcRetain: {
                const auto& rcRetain = static_cast<const RcRetainInst&>(*inst);
                if (!rcRetain.getPtr()) {
                    errors.push_back("rc_retain instruction has null pointer");
                    ok = false;
                }
                break;
            }
            case IROpcode::RcRelease: {
                const auto& rcRelease = static_cast<const RcReleaseInst&>(*inst);
                if (!rcRelease.getPtr()) {
                    errors.push_back("rc_release instruction has null pointer");
                    ok = false;
                }
                break;
            }
            default:
                break;
        }
    }

    return ok;
}

} // namespace sph

