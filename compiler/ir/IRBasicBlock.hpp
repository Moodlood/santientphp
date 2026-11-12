#pragma once

#include "IRInstruction.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sph {

class IRFunction;

class IRBasicBlock {
public:
    IRBasicBlock(std::string name, IRFunction* parent)
        : m_name(std::move(name)), m_parent(parent) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] IRFunction* getParent() const noexcept { return m_parent; }
    void setParent(IRFunction* parent) noexcept { m_parent = parent; }

    void addInstruction(std::shared_ptr<IRInstruction> inst);

    [[nodiscard]] const std::vector<std::shared_ptr<IRInstruction>>& getInstructions() const noexcept {
        return m_instructions;
    }
    [[nodiscard]] std::vector<std::shared_ptr<IRInstruction>>& getInstructions() noexcept {
        return m_instructions;
    }

    [[nodiscard]] bool hasTerminator() const noexcept;
    [[nodiscard]] const IRInstruction* getTerminator() const noexcept;
    [[nodiscard]] IRInstruction* getTerminator() noexcept;

private:
    std::string m_name;
    IRFunction* m_parent;
    std::vector<std::shared_ptr<IRInstruction>> m_instructions;
};

} // namespace sph

