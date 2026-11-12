#pragma once

#include "Pass.hpp"
#include <unordered_map>
#include <unordered_set>

namespace sph {

class DeadCodeEliminationPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "DeadCodeEliminationPass";
    }

    bool runOnFunction(IRFunction& fn) override;

private:
    bool eliminatePostTerminatorCode(IRFunction& fn);
    bool eliminateUnreachableBlocks(IRFunction& fn);
    bool eliminateDeadInstructions(IRFunction& fn);
    static bool isSideEffectFree(const IRInstruction& inst) noexcept;
};

} // namespace sph

