#pragma once

#include "Pass.hpp"

namespace sph {

class LoopInvariantCodeMotionPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "LoopInvariantCodeMotionPass";
    }

    bool runOnFunction(IRFunction& fn) override;
};

} // namespace sph

