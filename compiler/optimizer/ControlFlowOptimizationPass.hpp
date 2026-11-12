#pragma once

#include "Pass.hpp"

namespace sph {

class ControlFlowOptimizationPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "ControlFlowOptimizationPass";
    }

    bool runOnFunction(IRFunction& fn) override;

private:
    bool simplifyConditionalBranches(IRFunction& fn);
    bool bypassTrampolineBlocks(IRFunction& fn);
};

} // namespace sph

