#include "StrengthReductionPass.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRInstruction.hpp"
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

std::shared_ptr<IRValue> StrengthReductionPass::reduceBinary(BinaryInst& bin, bool& inPlaceChanged) {
    auto lhs = bin.getLhs();
    auto rhs = bin.getRhs();
    if (!lhs || !rhs) return nullptr;

    auto cLhsInt = dynamic_cast<const ConstantInt*>(lhs.get());
    auto cRhsInt = dynamic_cast<const ConstantInt*>(rhs.get());

    if (!bin.getType()->isInteger()) return nullptr;

    switch (bin.getOpcode()) {
        case IROpcode::Mul: {
            if (cRhsInt && isPowerOfTwo(cRhsInt->getValue())) {
                int shift = countTrailingZeros(cRhsInt->getValue());
                bin.setOpcode(IROpcode::ShiftLeft);
                bin.setRhs(ConstantInt::get(shift, bin.getType()));
                inPlaceChanged = true;
                return nullptr;
            }
            if (cLhsInt && isPowerOfTwo(cLhsInt->getValue())) {
                int shift = countTrailingZeros(cLhsInt->getValue());
                bin.setOpcode(IROpcode::ShiftLeft);
                bin.setLhs(rhs);
                bin.setRhs(ConstantInt::get(shift, bin.getType()));
                inPlaceChanged = true;
                return nullptr;
            }
            break;
        }

        case IROpcode::Div: {
            // For power-of-two division, if dividend >= 0
            if (cRhsInt && isPowerOfTwo(cRhsInt->getValue()) && cRhsInt->getValue() > 1) {
                if (cLhsInt && cLhsInt->getValue() >= 0) {
                    int shift = countTrailingZeros(cRhsInt->getValue());
                    bin.setOpcode(IROpcode::ShiftRight);
                    bin.setRhs(ConstantInt::get(shift, bin.getType()));
                    inPlaceChanged = true;
                    return nullptr;
                }
            }
            break;
        }

        case IROpcode::Mod: {
            // x % 2^k -> x & (2^k - 1) when x >= 0
            if (cRhsInt && isPowerOfTwo(cRhsInt->getValue())) {
                if (cLhsInt && cLhsInt->getValue() >= 0) {
                    int64_t mask = cRhsInt->getValue() - 1;
                    bin.setOpcode(IROpcode::BitwiseAnd);
                    bin.setRhs(ConstantInt::get(mask, bin.getType()));
                    inPlaceChanged = true;
                    return nullptr;
                }
            }
            break;
        }

        default:
            break;
    }

    return nullptr;
}

bool StrengthReductionPass::runOnFunction(IRFunction& fn) {
    bool changed = false;

    for (auto& bb : fn.getBlocks()) {
        for (auto& inst : bb->getInstructions()) {
            if (auto* bin = dynamic_cast<BinaryInst*>(inst.get())) {
                bool inPlaceChanged = false;
                auto replacement = reduceBinary(*bin, inPlaceChanged);
                if (inPlaceChanged) {
                    changed = true;
                } else if (replacement) {
                    const IRValue* targetValue = inst.get();
                    for (auto& otherBb : fn.getBlocks()) {
                        for (auto& otherInst : otherBb->getInstructions()) {
                            if (otherInst.get() != targetValue) {
                                replaceOperandInInstruction(*otherInst, targetValue, replacement);
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

