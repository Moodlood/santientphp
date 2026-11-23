#pragma once

#include "Pass.hpp"
#include <string_view>

namespace sph {

class Mem2RegPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "Mem2RegPass";
    }

    bool runOnFunction(IRFunction& function) override;
};

} // namespace sph

