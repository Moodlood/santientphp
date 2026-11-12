#pragma once

#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRModule.hpp"
#include <string_view>

namespace sph {

enum class OptimizationLevel {
    O0, // No optimization
    O1, // Basic optimization: constant folding, algebraic simplification, simple DCE
    O2, // Standard optimization: iterative folding, CSE, control flow, full DCE
    O3, // Aggressive optimization: O2 + function inlining
    Os  // Size-optimized: O2 with size-conscious thresholds
};

[[nodiscard]] inline const char* optimizationLevelToString(OptimizationLevel level) noexcept {
    switch (level) {
        case OptimizationLevel::O0: return "O0";
        case OptimizationLevel::O1: return "O1";
        case OptimizationLevel::O2: return "O2";
        case OptimizationLevel::O3: return "O3";
        case OptimizationLevel::Os: return "Os";
    }
    return "O0";
}

class Pass {
public:
    virtual ~Pass() = default;
    [[nodiscard]] virtual std::string_view getName() const noexcept = 0;
};

class FunctionPass : public Pass {
public:
    virtual bool runOnFunction(IRFunction& fn) = 0;
};

class ModulePass : public Pass {
public:
    virtual bool runOnModule(IRModule& module) = 0;
};

} // namespace sph

