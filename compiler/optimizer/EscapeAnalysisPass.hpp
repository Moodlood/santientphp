#pragma once

#include "Pass.hpp"
#include <string_view>

namespace sph {

class EscapeAnalysisPass : public FunctionPass {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "EscapeAnalysisPass";
    }

    bool runOnFunction(IRFunction& function) override;
};

} // namespace sph

