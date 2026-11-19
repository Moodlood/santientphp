#pragma once

#include "Pass.hpp"

namespace sph {

class DeadStoreEliminationPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "DeadStoreEliminationPass";
    }

    bool runOnFunction(IRFunction& fn) override;
};

} // namespace sph

