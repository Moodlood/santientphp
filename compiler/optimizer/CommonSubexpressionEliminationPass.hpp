#pragma once

#include "Pass.hpp"

namespace sph {

class CommonSubexpressionEliminationPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "CommonSubexpressionEliminationPass";
    }

    bool runOnFunction(IRFunction& fn) override;

private:
    static bool areEquivalent(const IRInstruction& a, const IRInstruction& b) noexcept;
    static bool isCommutative(IROpcode op) noexcept;
};

} // namespace sph

