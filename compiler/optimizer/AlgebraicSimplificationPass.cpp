#include "AlgebraicSimplificationPass.hpp"
#include <bit>

namespace sph {

namespace {

bool isPowerOfTwo(int64_t val) {
    return val > 0 && (val & (val - 1)) == 0;
}

int countTrailingZeros(int64_t val) {
    int count = 0;
    while ((val & 1) == 0 && count < 64) {
        val >>= 1;
        ++count;
    }
    return count;
}

} // namespace

std::shared_ptr<IRValue> AlgebraicSimplificationPass::simplifyBinary(BinaryInst& bin, bool& inPlaceChanged) {
    auto lhs = bin.getLhs();
    auto rhs = bin.getRhs();
    if (!lhs || !rhs) return nullptr;

    auto cLhsInt = dynamic_cast<const ConstantInt*>(lhs.get());
    auto cRhsInt = dynamic_cast<const ConstantInt*>(rhs.get());
    auto cLhsBool = dynamic_cast<const ConstantBool*>(lhs.get());
    auto cRhsBool = dynamic_cast<const ConstantBool*>(rhs.get());

    bool sameOperands = (lhs.get() == rhs.get());

    switch (bin.getOpcode()) {
        case IROpcode::Add:
            // x + 0 -> x
            if (cRhsInt && cRhsInt->getValue() == 0) return lhs;
            // 0 + x -> x
            if (cLhsInt && cLhsInt->getValue() == 0) return rhs;
            break;

        case IROpcode::Sub:
            // x - 0 -> x
            if (cRhsInt && cRhsInt->getValue() == 0) return lhs;
            // x - x -> 0 (only valid for integers, not for IEEE-754 floats where NaN - NaN is NaN and Inf - Inf is NaN)
            if (sameOperands && bin.getType()->isInteger()) return ConstantInt::get(0, bin.getType());
            break;

        case IROpcode::Mul:
            // x * 0 -> 0 and 0 * x -> 0 (only for integers; for floats, Inf * 0 is NaN and NaN * 0 is NaN)
            if (bin.getType()->isInteger()) {
                if (cRhsInt && cRhsInt->getValue() == 0) return rhs;
                if (cLhsInt && cLhsInt->getValue() == 0) return lhs;
            }
            // x * 1 -> x
            if (cRhsInt && cRhsInt->getValue() == 1) return lhs;
            // 1 * x -> x
            if (cLhsInt && cLhsInt->getValue() == 1) return rhs;

            // Strength reduction: x * 2^k -> x << k (valid for integers)
            if (bin.getType()->isInteger()) {
                if (cRhsInt && isPowerOfTwo(cRhsInt->getValue())) {
                    int shift = countTrailingZeros(cRhsInt->getValue());
                    bin.setOpcode(IROpcode::ShiftLeft);
                    bin.setRhs(ConstantInt::get(shift, bin.getType()));
                    inPlaceChanged = true;
                    return nullptr; // Transformed in place
                }
                if (cLhsInt && isPowerOfTwo(cLhsInt->getValue())) {
                    int shift = countTrailingZeros(cLhsInt->getValue());
                    bin.setOpcode(IROpcode::ShiftLeft);
                    bin.setLhs(rhs);
                    bin.setRhs(ConstantInt::get(shift, bin.getType()));
                    inPlaceChanged = true;
                    return nullptr; // Transformed in place
                }
            }
            break;

        case IROpcode::Div:
            // x / 1 -> x
            if (cRhsInt && cRhsInt->getValue() == 1) return lhs;
            // 0 / x -> 0 (only when x is known non-zero constant, avoiding division by zero elimination)
            if (cLhsInt && cLhsInt->getValue() == 0 && cRhsInt && cRhsInt->getValue() != 0) return lhs;
            // x / x -> 1 (only when x is known non-zero integer constant; folding variable x / x when x == 0 silently drops hardware exception)
            if (sameOperands && cLhsInt && cLhsInt->getValue() != 0 && bin.getType()->isInteger()) {
                return ConstantInt::get(1, bin.getType());
            }

            // Strength reduction: x / 2^k -> x >> k
            // For signed integers, SAR rounds towards -infinity (-3 >> 1 == -2) while integer division
            // truncates toward zero (-3 / 2 == -1). Only reduce when dividend is provably non-negative.
            if (bin.getType()->isInteger() && cRhsInt && isPowerOfTwo(cRhsInt->getValue()) && cRhsInt->getValue() > 1) {
                if (cLhsInt && cLhsInt->getValue() >= 0) {
                    int shift = countTrailingZeros(cRhsInt->getValue());
                    bin.setOpcode(IROpcode::ShiftRight);
                    bin.setRhs(ConstantInt::get(shift, bin.getType()));
                    inPlaceChanged = true;
                    return nullptr;
                }
            }
            break;

        case IROpcode::BitwiseAnd:
            // x & 0 -> 0
            if (cRhsInt && cRhsInt->getValue() == 0) return rhs;
            if (cLhsInt && cLhsInt->getValue() == 0) return lhs;
            // x & x -> x
            if (sameOperands) return lhs;
            // x & -1 -> x
            if (cRhsInt && cRhsInt->getValue() == -1) return lhs;
            if (cLhsInt && cLhsInt->getValue() == -1) return rhs;
            break;

        case IROpcode::BitwiseOr:
            // x | 0 -> x
            if (cRhsInt && cRhsInt->getValue() == 0) return lhs;
            if (cLhsInt && cLhsInt->getValue() == 0) return rhs;
            // x | x -> x
            if (sameOperands) return lhs;
            break;

        case IROpcode::BitwiseXor:
            // x ^ 0 -> x
            if (cRhsInt && cRhsInt->getValue() == 0) return lhs;
            if (cLhsInt && cLhsInt->getValue() == 0) return rhs;
            // x ^ x -> 0
            if (sameOperands) return ConstantInt::get(0, bin.getType());
            break;

        case IROpcode::ShiftLeft:
        case IROpcode::ShiftRight:
            // x << 0 -> x, x >> 0 -> x
            if (cRhsInt && cRhsInt->getValue() == 0) return lhs;
            // 0 << x -> 0, 0 >> x -> 0
            if (cLhsInt && cLhsInt->getValue() == 0) return lhs;
            break;

        case IROpcode::Equal:
            // x == x -> true
            if (sameOperands && !lhs->getType()->isFloatingPoint()) return ConstantBool::get(true);
            break;

        case IROpcode::NotEqual:
            // x != x -> false
            if (sameOperands && !lhs->getType()->isFloatingPoint()) return ConstantBool::get(false);
            break;

        case IROpcode::LessEqual:
        case IROpcode::GreaterEqual:
            if (sameOperands && !lhs->getType()->isFloatingPoint()) return ConstantBool::get(true);
            break;

        case IROpcode::Less:
        case IROpcode::Greater:
            if (sameOperands && !lhs->getType()->isFloatingPoint()) return ConstantBool::get(false);
            break;

        case IROpcode::LogicalAnd:
            if (cRhsBool && cRhsBool->getValue()) return lhs;
            if (cLhsBool && cLhsBool->getValue()) return rhs;
            if (cRhsBool && !cRhsBool->getValue()) return rhs;
            if (cLhsBool && !cLhsBool->getValue()) return lhs;
            if (sameOperands) return lhs;
            break;

        case IROpcode::LogicalOr:
            if (cRhsBool && !cRhsBool->getValue()) return lhs;
            if (cLhsBool && !cLhsBool->getValue()) return rhs;
            if (cRhsBool && cRhsBool->getValue()) return rhs;
            if (cLhsBool && cLhsBool->getValue()) return lhs;
            if (sameOperands) return lhs;
            break;

        default:
            break;
    }

    return nullptr;
}

bool AlgebraicSimplificationPass::runOnFunction(IRFunction& fn) {
    bool changed = false;

    for (auto& bb : fn.getBlocks()) {
        auto& instructions = bb->getInstructions();
        for (auto& inst : instructions) {
            if (inst->getOpcode() >= IROpcode::Add && inst->getOpcode() <= IROpcode::ShiftRight) {
                auto bin = std::static_pointer_cast<BinaryInst>(inst);
                bool inPlaceChanged = false;
                auto simplified = simplifyBinary(*bin, inPlaceChanged);
                if (inPlaceChanged) {
                    changed = true;
                }
                if (simplified) {
                    const IRValue* targetValue = inst.get();
                    for (auto& otherBb : fn.getBlocks()) {
                        for (auto& otherInst : otherBb->getInstructions()) {
                            if (otherInst.get() != targetValue) {
                                replaceOperandInInstruction(*otherInst, targetValue, simplified);
                            }
                        }
                    }
                    changed = true;
                }
            }
        }
    }

    return changed;
}

} // namespace sph

