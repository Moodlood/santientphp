#include "CommonSubexpressionEliminationPass.hpp"
#include <vector>

namespace sph {

bool CommonSubexpressionEliminationPass::isCommutative(IROpcode op) noexcept {
    switch (op) {
        case IROpcode::Add:
        case IROpcode::Mul:
        case IROpcode::BitwiseAnd:
        case IROpcode::BitwiseOr:
        case IROpcode::BitwiseXor:
        case IROpcode::Equal:
        case IROpcode::NotEqual:
            return true;
        default:
            return false;
    }
}

bool CommonSubexpressionEliminationPass::areEquivalent(const IRInstruction& a, const IRInstruction& b) noexcept {
    if (a.getOpcode() != b.getOpcode()) return false;
    if (a.getType() != b.getType()) return false;

    if (a.getOpcode() >= IROpcode::Add && a.getOpcode() <= IROpcode::ShiftRight) {
        const auto& binA = static_cast<const BinaryInst&>(a);
        const auto& binB = static_cast<const BinaryInst&>(b);

        bool directMatch = (binA.getLhs().get() == binB.getLhs().get() &&
                            binA.getRhs().get() == binB.getRhs().get());
        if (directMatch) return true;

        if (isCommutative(a.getOpcode())) {
            return (binA.getLhs().get() == binB.getRhs().get() &&
                    binA.getRhs().get() == binB.getLhs().get());
        }
        return false;
    }

    if (a.getOpcode() == IROpcode::Negate ||
        a.getOpcode() == IROpcode::LogicalNot ||
        a.getOpcode() == IROpcode::BitwiseNot) {
        const auto& unA = static_cast<const UnaryInst&>(a);
        const auto& unB = static_cast<const UnaryInst&>(b);
        return unA.getOperand().get() == unB.getOperand().get();
    }

    if (a.getOpcode() == IROpcode::Cast) {
        const auto& castA = static_cast<const CastInst&>(a);
        const auto& castB = static_cast<const CastInst&>(b);
        return castA.getOperand().get() == castB.getOperand().get();
    }

    return false;
}

bool CommonSubexpressionEliminationPass::runOnFunction(IRFunction& fn) {
    bool changed = false;

    for (auto& bb : fn.getBlocks()) {
        auto& insts = bb->getInstructions();
        std::vector<std::shared_ptr<IRInstruction>> available;

        for (size_t i = 0; i < insts.size(); ++i) {
            auto& curr = insts[i];
            bool isPure = (curr->getOpcode() >= IROpcode::Add && curr->getOpcode() <= IROpcode::ShiftRight) ||
                          (curr->getOpcode() >= IROpcode::Negate && curr->getOpcode() <= IROpcode::BitwiseNot) ||
                          (curr->getOpcode() == IROpcode::Cast);

            if (!isPure) continue;

            std::shared_ptr<IRValue> existing = nullptr;
            for (const auto& prev : available) {
                if (areEquivalent(*curr, *prev)) {
                    existing = prev;
                    break;
                }
            }

            if (existing) {
                const IRValue* target = curr.get();
                for (auto& otherBb : fn.getBlocks()) {
                    for (auto& otherInst : otherBb->getInstructions()) {
                        if (otherInst.get() != target) {
                            replaceOperandInInstruction(*otherInst, target, existing);
                        }
                    }
                }
                changed = true;
            } else {
                available.push_back(curr);
            }
        }
    }

    return changed;
}

} // namespace sph

