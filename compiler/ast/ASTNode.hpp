#pragma once

#include "compiler/source/SourceLocation.hpp"

namespace sph {

class ASTVisitor;

class ASTNode {
public:
    virtual ~ASTNode() = default;

    [[nodiscard]] const SourceSpan& getSpan() const noexcept { return m_span; }
    void setSpan(SourceSpan span) noexcept { m_span = span; }

    virtual void accept(ASTVisitor& visitor) = 0;

protected:
    explicit ASTNode(SourceSpan span) : m_span(span) {}

private:
    SourceSpan m_span;
};

} // namespace sph

