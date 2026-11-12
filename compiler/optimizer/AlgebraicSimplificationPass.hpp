#pragma once

#include "Pass.hpp"

namespace sph {

class AlgebraicSimplificationPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "AlgebraicSimplificationPass";
    }

    bool runOnFunction(IRFunction& fn) override;

private:
    std::shared_ptr<IRValue> simplifyBinary(BinaryInst& bin, bool& inPlaceChanged);
};

} // namespace sph

