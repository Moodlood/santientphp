#pragma once

#include "Pass.hpp"
#include <memory>
#include <vector>

namespace sph {

class PassManager {
public:
    explicit PassManager(OptimizationLevel level = OptimizationLevel::O0)
        : m_level(level) {}

    void addFunctionPass(std::unique_ptr<FunctionPass> pass);
    void addModulePass(std::unique_ptr<ModulePass> pass);

    [[nodiscard]] OptimizationLevel getLevel() const noexcept { return m_level; }
    void setLevel(OptimizationLevel level) noexcept { m_level = level; }

    bool run(IRModule& module);

    [[nodiscard]] static PassManager createForLevel(OptimizationLevel level);

private:
    OptimizationLevel m_level;
    std::vector<std::unique_ptr<FunctionPass>> m_functionPasses;
    std::vector<std::unique_ptr<ModulePass>> m_modulePasses;
};

} // namespace sph

