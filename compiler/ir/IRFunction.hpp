#pragma once

#include "IRBasicBlock.hpp"
#include "compiler/source/SourceLocation.hpp"
#include "compiler/types/Type.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sph {

class IRFunction {
public:
    IRFunction(std::string name, std::shared_ptr<Type> returnType,
               std::vector<std::shared_ptr<IRParameter>> params)
        : m_name(std::move(name)), m_returnType(std::move(returnType)),
          m_parameters(std::move(params)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] const std::shared_ptr<Type>& getReturnType() const noexcept { return m_returnType; }
    [[nodiscard]] const std::vector<std::shared_ptr<IRParameter>>& getParameters() const noexcept {
        return m_parameters;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<IRBasicBlock>>& getBlocks() const noexcept {
        return m_blocks;
    }
    [[nodiscard]] std::vector<std::unique_ptr<IRBasicBlock>>& getBlocks() noexcept {
        return m_blocks;
    }

    [[nodiscard]] const SourceLocation& getLocation() const noexcept { return m_loc; }
    void setLocation(SourceLocation loc) noexcept { m_loc = loc; }

    IRBasicBlock* createBlock(const std::string& nameHint = "bb");
    [[nodiscard]] IRBasicBlock* getEntryBlock() const noexcept;
    [[nodiscard]] bool isDeclaration() const noexcept { return m_blocks.empty(); }

private:
    std::string m_name;
    std::shared_ptr<Type> m_returnType;
    std::vector<std::shared_ptr<IRParameter>> m_parameters;
    std::vector<std::unique_ptr<IRBasicBlock>> m_blocks;
    size_t m_blockCounter = 0;
    SourceLocation m_loc;
};

} // namespace sph

