#include "ConstantFoldingPass.hpp"
#include <cmath>

namespace sph {

std::shared_ptr<IRValue> ConstantFoldingPass::foldBinary(const BinaryInst& bin) {
    auto lhs = bin.getLhs();
    auto rhs = bin.getRhs();
    if (!lhs || !rhs) return nullptr;

    // Integer constant folding
    if (auto cLhs = dynamic_cast<const ConstantInt*>(lhs.get())) {
        if (auto cRhs = dynamic_cast<const ConstantInt*>(rhs.get())) {
            int64_t a = cLhs->getValue();
            int64_t b = cRhs->getValue();

            switch (bin.getOpcode()) {
                case IROpcode::Add: return ConstantInt::get(a + b, bin.getType());
                case IROpcode::Sub: return ConstantInt::get(a - b, bin.getType());
                case IROpcode::Mul: return ConstantInt::get(a * b, bin.getType());
                case IROpcode::Div:
                    if (b != 0) return ConstantInt::get(a / b, bin.getType());
                    break;
                case IROpcode::Mod:
                    if (b != 0) return ConstantInt::get(a % b, bin.getType());
                    break;
                case IROpcode::BitwiseAnd: return ConstantInt::get(a & b, bin.getType());
                case IROpcode::BitwiseOr:  return ConstantInt::get(a | b, bin.getType());
                case IROpcode::BitwiseXor: return ConstantInt::get(a ^ b, bin.getType());
                case IROpcode::ShiftLeft:  return ConstantInt::get(a << (b & 63), bin.getType());
                case IROpcode::ShiftRight: return ConstantInt::get(a >> (b & 63), bin.getType());

                case IROpcode::Equal:        return ConstantBool::get(a == b);
                case IROpcode::NotEqual:     return ConstantBool::get(a != b);
                case IROpcode::Less:         return ConstantBool::get(a < b);
                case IROpcode::LessEqual:    return ConstantBool::get(a <= b);
                case IROpcode::Greater:      return ConstantBool::get(a > b);
                case IROpcode::GreaterEqual: return ConstantBool::get(a >= b);

                case IROpcode::LogicalAnd:   return ConstantBool::get((a != 0) && (b != 0));
                case IROpcode::LogicalOr:    return ConstantBool::get((a != 0) || (b != 0));

                default:
                    break;
            }
        }
    }

    // Floating point constant folding
    if (auto cLhs = dynamic_cast<const ConstantFloat*>(lhs.get())) {
        if (auto cRhs = dynamic_cast<const ConstantFloat*>(rhs.get())) {
            double a = cLhs->getValue();
            double b = cRhs->getValue();

            switch (bin.getOpcode()) {
                case IROpcode::Add: return ConstantFloat::get(a + b, bin.getType());
                case IROpcode::Sub: return ConstantFloat::get(a - b, bin.getType());
                case IROpcode::Mul: return ConstantFloat::get(a * b, bin.getType());
                case IROpcode::Div:
                    if (b != 0.0) return ConstantFloat::get(a / b, bin.getType());
                    break;

                case IROpcode::Equal:        return ConstantBool::get(a == b);
                case IROpcode::NotEqual:     return ConstantBool::get(a != b);
                case IROpcode::Less:         return ConstantBool::get(a < b);
                case IROpcode::LessEqual:    return ConstantBool::get(a <= b);
                case IROpcode::Greater:      return ConstantBool::get(a > b);
                case IROpcode::GreaterEqual: return ConstantBool::get(a >= b);

                default:
                    break;
            }
        }
    }

    // Boolean constant folding
    if (auto cLhs = dynamic_cast<const ConstantBool*>(lhs.get())) {
        if (auto cRhs = dynamic_cast<const ConstantBool*>(rhs.get())) {
            bool a = cLhs->getValue();
            bool b = cRhs->getValue();

            switch (bin.getOpcode()) {
                case IROpcode::Equal:        return ConstantBool::get(a == b);
                case IROpcode::NotEqual:     return ConstantBool::get(a != b);
                case IROpcode::LogicalAnd:   return ConstantBool::get(a && b);
                case IROpcode::LogicalOr:    return ConstantBool::get(a || b);
                default:
                    break;
            }
        }
    }

    return nullptr;
}

std::shared_ptr<IRValue> ConstantFoldingPass::foldUnary(const UnaryInst& un) {
    auto op = un.getOperand();
    if (!op) return nullptr;

    if (auto cInt = dynamic_cast<const ConstantInt*>(op.get())) {
        int64_t val = cInt->getValue();
        switch (un.getOpcode()) {
            case IROpcode::Negate:     return ConstantInt::get(-val, un.getType());
            case IROpcode::LogicalNot: return ConstantBool::get(val == 0);
            case IROpcode::BitwiseNot: return ConstantInt::get(~val, un.getType());
            default: break;
        }
    } else if (auto cFloat = dynamic_cast<const ConstantFloat*>(op.get())) {
        double val = cFloat->getValue();
        if (un.getOpcode() == IROpcode::Negate) {
            return ConstantFloat::get(-val, un.getType());
        }
    } else if (auto cBool = dynamic_cast<const ConstantBool*>(op.get())) {
        bool val = cBool->getValue();
        if (un.getOpcode() == IROpcode::LogicalNot) {
            return ConstantBool::get(!val);
        }
    }

    return nullptr;
}

std::shared_ptr<IRValue> ConstantFoldingPass::foldCast(const CastInst& cast) {
    auto op = cast.getOperand();
    if (!op) return nullptr;

    if (auto cInt = dynamic_cast<const ConstantInt*>(op.get())) {
        int64_t val = cInt->getValue();
        if (cast.getType()->isInteger()) {
            return ConstantInt::get(val, cast.getType());
        }
        if (cast.getType()->isFloatingPoint()) {
            return ConstantFloat::get(static_cast<double>(val), cast.getType());
        }
        if (cast.getType()->isBoolean()) {
            return ConstantBool::get(val != 0);
        }
    } else if (auto cFloat = dynamic_cast<const ConstantFloat*>(op.get())) {
        double val = cFloat->getValue();
        if (cast.getType()->isInteger()) {
            return ConstantInt::get(static_cast<int64_t>(val), cast.getType());
        }
        if (cast.getType()->isFloatingPoint()) {
            return ConstantFloat::get(val, cast.getType());
        }
    } else if (auto cBool = dynamic_cast<const ConstantBool*>(op.get())) {
        bool val = cBool->getValue();
        if (cast.getType()->isInteger()) {
            return ConstantInt::get(val ? 1 : 0, cast.getType());
        }
        if (cast.getType()->isBoolean()) {
            return ConstantBool::get(val);
        }
    }

    return nullptr;
}

bool ConstantFoldingPass::runOnFunction(IRFunction& fn) {
    bool changed = false;

    for (auto& bb : fn.getBlocks()) {
        auto& instructions = bb->getInstructions();
        for (size_t i = 0; i < instructions.size(); ++i) {
            auto& inst = instructions[i];
            std::shared_ptr<IRValue> folded = nullptr;

            if (inst->getOpcode() == IROpcode::CondBr) {
                auto condBr = std::static_pointer_cast<CondBranchInst>(inst);
                if (auto cBool = dynamic_cast<const ConstantBool*>(condBr->getCondition().get())) {
                    IRBasicBlock* target = cBool->getValue() ? condBr->getTrueTarget() : condBr->getFalseTarget();
                    instructions[i] = std::make_shared<BranchInst>(target);
                    changed = true;
                    continue;
                }
                if (auto cInt = dynamic_cast<const ConstantInt*>(condBr->getCondition().get())) {
                    IRBasicBlock* target = (cInt->getValue() != 0) ? condBr->getTrueTarget() : condBr->getFalseTarget();
                    instructions[i] = std::make_shared<BranchInst>(target);
                    changed = true;
                    continue;
                }
            } else if (inst->getOpcode() == IROpcode::Cast) {
                folded = foldCast(*std::static_pointer_cast<CastInst>(inst));
            } else if (inst->getOpcode() == IROpcode::Negate ||
                       inst->getOpcode() == IROpcode::LogicalNot ||
                       inst->getOpcode() == IROpcode::BitwiseNot) {
                folded = foldUnary(*std::static_pointer_cast<UnaryInst>(inst));
            } else if (inst->getOpcode() >= IROpcode::Add && inst->getOpcode() <= IROpcode::ShiftRight) {
                folded = foldBinary(*std::static_pointer_cast<BinaryInst>(inst));
            }

            if (folded) {
                const IRValue* targetValue = inst.get();
                // Replace all uses across all basic blocks in the function
                for (auto& otherBb : fn.getBlocks()) {
                    for (auto& otherInst : otherBb->getInstructions()) {
                        if (otherInst.get() != targetValue) {
                            replaceOperandInInstruction(*otherInst, targetValue, folded);
                        }
                    }
                }
                changed = true;
            }
        }
    }

    return changed;
}

} // namespace sph

