#include "IRBasicBlock.hpp"

namespace sph {

void IRBasicBlock::addInstruction(std::shared_ptr<IRInstruction> inst) {
    if (inst) {
        inst->setParent(this);
        m_instructions.push_back(std::move(inst));
    }
}

bool IRBasicBlock::hasTerminator() const noexcept {
    return !m_instructions.empty() && m_instructions.back()->isTerminator();
}

const IRInstruction* IRBasicBlock::getTerminator() const noexcept {
    if (hasTerminator()) {
        return m_instructions.back().get();
    }
    return nullptr;
}

IRInstruction* IRBasicBlock::getTerminator() noexcept {
    if (hasTerminator()) {
        return m_instructions.back().get();
    }
    return nullptr;
}

} // namespace sph

