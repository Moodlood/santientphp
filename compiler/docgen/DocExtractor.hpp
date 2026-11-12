#pragma once

#include "compiler/ast/ASTNode.hpp"
#include "compiler/ast/DeclAST.hpp"
#include "compiler/docgen/DocModel.hpp"
#include "compiler/source/SourceManager.hpp"
#include <string>
#include <vector>

namespace sph {

class DocExtractor {
public:
    explicit DocExtractor(const SourceManager& sourceManager)
        : m_sourceManager(sourceManager) {}

    // Extract documentation from a parsed program AST
    [[nodiscard]] DocModule extractModule(uint32_t fileId, const ProgramAST& program);

private:
    const SourceManager& m_sourceManager;

    // Find and parse doc comment immediately preceding a line number (1-based)
    [[nodiscard]] DocComment findDocCommentAbove(uint32_t fileId, uint32_t line) const;

    [[nodiscard]] DocFunction extractFunction(uint32_t fileId, const FunctionDecl& fn) const;
    [[nodiscard]] DocStruct extractStruct(uint32_t fileId, const StructDecl& st) const;
    [[nodiscard]] DocClass extractClass(uint32_t fileId, const ClassDecl& cl) const;
    [[nodiscard]] DocInterface extractInterface(uint32_t fileId, const InterfaceDecl& in) const;
    [[nodiscard]] DocEnum extractEnum(uint32_t fileId, const EnumDecl& en) const;
};

} // namespace sph

