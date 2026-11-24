#include "IRBasicBlock.hpp"

namespace sph {

void IRBasicBlock::addInstruction(std::shared_ptr<IRInstruction> inst) {
    if (inst) {
        inst->setParent(this);
        m_instructions.push_back(std::move(inst));
    }
}

void IRBasicBlock::appendInstruction(std::shared_ptr<IRInstruction> inst) {
    addInstruction(std::move(inst));
}

void IRBasicBlock::insertInstruction(size_t index, std::shared_ptr<IRInstruction> inst) {
    if (!inst) return;
    inst->setParent(this);
    if (index >= m_instructions.size()) {
        m_instructions.push_back(std::move(inst));
    } else {
        m_instructions.insert(m_instructions.begin() + index, std::move(inst));
    }
}

void IRBasicBlock::replaceInstruction(size_t index, std::shared_ptr<IRInstruction> inst) {
    if (!inst || index >= m_instructions.size()) return;
    inst->setParent(this);
    if (m_instructions[index]) {
        m_instructions[index]->setParent(nullptr);
    }
    m_instructions[index] = std::move(inst);
}

void IRBasicBlock::removeInstruction(size_t index) {
    if (index < m_instructions.size()) {
        if (m_instructions[index]) {
            m_instructions[index]->setParent(nullptr);
        }
        m_instructions.erase(m_instructions.begin() + index);
    }
}

void IRBasicBlock::moveInstructionTo(IRBasicBlock& targetBlock, size_t fromIdx, size_t toIdx) {
    if (fromIdx >= m_instructions.size()) return;
    auto inst = m_instructions[fromIdx];
    m_instructions.erase(m_instructions.begin() + fromIdx);
    targetBlock.insertInstruction(toIdx, inst);
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

