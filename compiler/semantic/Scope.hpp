#pragma once

#include "Symbol.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace sph {

class Scope {
public:
    explicit Scope(Scope* parent = nullptr) : m_parent(parent) {}

    [[nodiscard]] Scope* getParent() const noexcept { return m_parent; }

    bool define(Symbol sym);

    [[nodiscard]] const Symbol* lookup(const std::string& name) const;
    [[nodiscard]] Symbol* lookup(const std::string& name);

    [[nodiscard]] const Symbol* lookupCurrent(const std::string& name) const;

private:
    Scope* m_parent;
    std::unordered_map<std::string, Symbol> m_symbols;
};

} // namespace sph

