#pragma once

#include "Pass.hpp"
#include <string_view>

namespace sph {

class TailCallPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "TailCallPass";
    }

    bool runOnFunction(IRFunction& function) override;
};

} // namespace sph

