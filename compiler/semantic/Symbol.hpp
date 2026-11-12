#pragma once

#include "compiler/source/SourceLocation.hpp"
#include "compiler/types/Type.hpp"
#include <memory>
#include <string>

namespace sph {

enum class SymbolKind {
    Variable,
    Function,
    Struct,
    Enum,
    Interface,
    Class
};

struct Symbol {
    SymbolKind kind = SymbolKind::Variable;
    std::string name;
    std::shared_ptr<Type> type;
    bool isMut = false;
    bool isMoved = false;
    SourceSpan span;

    Symbol() = default;
    Symbol(SymbolKind k, std::string n, std::shared_ptr<Type> t, bool mut, SourceSpan sp)
        : kind(k), name(std::move(n)), type(std::move(t)), isMut(mut), isMoved(false), span(sp) {}

    static Symbol makeVariable(std::string name, std::shared_ptr<Type> type, bool isMut, SourceSpan span) {
        return Symbol(SymbolKind::Variable, std::move(name), std::move(type), isMut, span);
    }

    static Symbol makeFunction(std::string name, std::shared_ptr<Type> type, SourceSpan span) {
        return Symbol(SymbolKind::Function, std::move(name), std::move(type), false, span);
    }

    static Symbol makeStruct(std::string name, std::shared_ptr<Type> type, SourceSpan span) {
        return Symbol(SymbolKind::Struct, std::move(name), std::move(type), false, span);
    }

    static Symbol makeEnum(std::string name, std::shared_ptr<Type> type, SourceSpan span) {
        return Symbol(SymbolKind::Enum, std::move(name), std::move(type), false, span);
    }

    static Symbol makeInterface(std::string name, std::shared_ptr<Type> type, SourceSpan span) {
        return Symbol(SymbolKind::Interface, std::move(name), std::move(type), false, span);
    }

    static Symbol makeClass(std::string name, std::shared_ptr<Type> type, SourceSpan span) {
        return Symbol(SymbolKind::Class, std::move(name), std::move(type), false, span);
    }
};

} // namespace sph

