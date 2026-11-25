#include "AlgebraicSimplificationPass.hpp"
#include <bit>
#include <unordered_map>

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

    // Count instruction uses across the function to avoid duplicating shared multipliers.
    std::unordered_map<const IRValue*, size_t> useCounts;
    for (const auto& bb : fn.getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            for (const auto* op : getInstructionOperands(*inst)) {
                if (op) useCounts[op]++;
            }
        }
    }

    auto matchScaled = [](const std::shared_ptr<IRValue>& v,
                          std::shared_ptr<IRValue>& outTerm,
                          std::shared_ptr<IRValue>& outScale,
                          bool& outIsShift) -> bool {
        auto* bin = dynamic_cast<BinaryInst*>(v.get());
        if (!bin || !bin->getType() || !bin->getType()->isInteger() || bin->getType()->isBoolean()) {
            return false;
        }
        if (bin->getOpcode() == IROpcode::Mul) {
            outTerm = bin->getLhs();
            outScale = bin->getRhs();
            outIsShift = false;
            return outTerm && outScale;
        }
        if (bin->getOpcode() == IROpcode::ShiftLeft) {
            if (auto* ci = dynamic_cast<const ConstantInt*>(bin->getRhs().get())) {
                outTerm = bin->getLhs();
                outScale = bin->getRhs();
                outIsShift = true;
                return outTerm && ci != nullptr;
            }
        }
        return false;
    };

    size_t factorCounter = 0;

    for (auto& bb : fn.getBlocks()) {
        auto& instructions = bb->getInstructions();
        for (size_t idx = 0; idx < instructions.size(); ++idx) {
            auto& inst = instructions[idx];

            // Factor distributive integer patterns:
            //   (a * k) + (b * k) -> (a + b) * k (including commutative operand orders)
            //   (a << c) + (b << c) -> (a + b) << c
            if (inst->getOpcode() == IROpcode::Add && inst->getType() &&
                inst->getType()->isInteger() && !inst->getType()->isBoolean()) {
                auto bin = std::static_pointer_cast<BinaryInst>(inst);
                auto lhsVal = bin->getLhs();
                auto rhsVal = bin->getRhs();
                std::shared_ptr<IRValue> x1, y1, x2, y2;
                bool shift1 = false, shift2 = false;
                if (matchScaled(lhsVal, x1, y1, shift1) && matchScaled(rhsVal, x2, y2, shift2) &&
                    shift1 == shift2 && lhsVal.get() != rhsVal.get() &&
                    useCounts[lhsVal.get()] <= 1 && useCounts[rhsVal.get()] <= 1) {
                    std::shared_ptr<IRValue> a, b, common;
                    if (!shift1) {
                        if (x1.get() == x2.get())      { a = y1; b = y2; common = x1; }
                        else if (x1.get() == y2.get()) { a = y1; b = x2; common = x1; }
                        else if (y1.get() == x2.get()) { a = x1; b = y2; common = y1; }
                        else if (y1.get() == y2.get()) { a = x1; b = x2; common = y1; }
                    } else {
                        auto* c1 = dynamic_cast<const ConstantInt*>(y1.get());
                        auto* c2 = dynamic_cast<const ConstantInt*>(y2.get());
                        if (c1 && c2 && c1->getValue() == c2->getValue()) {
                            a = x1; b = x2; common = y1;
                        }
                    }

                    if (common && a && b) {
                        auto sumInst = std::make_shared<BinaryInst>(
                            IROpcode::Add, a, b, bin->getType(),
                            ".factor." + std::to_string(factorCounter++));
                        sumInst->setParent(bb.get());
                        bin->setOpcode(shift1 ? IROpcode::ShiftLeft : IROpcode::Mul);
                        bin->setLhs(sumInst);
                        bin->setRhs(common);
                        bb->insertInstruction(idx, sumInst);
                        ++idx; // Advance past inserted sumInst
                        changed = true;
                        continue;
                    }
                }
            }

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

