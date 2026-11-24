#pragma once

#include "Pass.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include <string_view>

namespace sph {

// Helper to determine if an edge between src and dst is critical
// (src has > 1 successors AND dst has > 1 predecessors)
[[nodiscard]] bool isCriticalEdge(const IRBasicBlock& src, const IRBasicBlock& dst, const IRFunction& fn);

// Splits all critical edges in fn by inserting an intermediate trampoline block
// and updating branch targets and phi incoming references.
bool splitCriticalEdges(IRFunction& fn);

class CriticalEdgeSplittingPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "CriticalEdgeSplittingPass";
    }

    bool runOnFunction(IRFunction& function) override;
};

} // namespace sph

