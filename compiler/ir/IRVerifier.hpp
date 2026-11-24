#pragma once

#include "IRModule.hpp"
#include <string>
#include <vector>

namespace sph {

class DominatorTree;

class IRVerifier {
public:
    IRVerifier() = default;

    [[nodiscard]] bool verify(const IRModule& module, std::vector<std::string>& errors) const;
    [[nodiscard]] bool verify(const IRFunction& fn, std::vector<std::string>& errors) const;
    [[nodiscard]] bool verify(const IRBasicBlock& bb, const IRFunction& fn, std::vector<std::string>& errors) const;
    [[nodiscard]] bool verifySSA(const IRFunction& fn, const DominatorTree& domTree, std::vector<std::string>& errors) const;
};

} // namespace sph
