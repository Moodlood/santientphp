#pragma once

#include "IRModule.hpp"
#include <string>
#include <vector>

namespace sph {

class IRVerifier {
public:
    IRVerifier() = default;

    [[nodiscard]] bool verify(const IRModule& module, std::vector<std::string>& errors) const;
    [[nodiscard]] bool verify(const IRFunction& fn, std::vector<std::string>& errors) const;
    [[nodiscard]] bool verify(const IRBasicBlock& bb, const IRFunction& fn, std::vector<std::string>& errors) const;
};

} // namespace sph

