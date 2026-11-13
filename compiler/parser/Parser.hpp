#pragma once

#include "compiler/ast/DeclAST.hpp"
#include "compiler/ast/ExprAST.hpp"
#include "compiler/ast/StmtAST.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/lexer/Lexer.hpp"
#include "compiler/lexer/Token.hpp"
#include "compiler/source/SourceManager.hpp"
#include <memory>
#include <unordered_set>
#include <vector>

namespace sph {

class Parser {
public:
    Parser(uint32_t fileId, SourceManager& sourceManager, DiagnosticEmitter* diagnostics = nullptr,
           std::shared_ptr<std::unordered_set<std::string>> visitedFiles = nullptr);

    std::unique_ptr<ProgramAST> parseProgram();

private:
    // Navigation & Token helpers
    [[nodiscard]] bool isAtEnd() const noexcept;
    [[nodiscard]] const Token& peek() const noexcept;
    [[nodiscard]] const Token& peekNext() const noexcept;
    [[nodiscard]] const Token& previous() const noexcept;
    const Token& advance() noexcept;
    [[nodiscard]] bool check(TokenKind kind) const noexcept;
    bool match(TokenKind kind) noexcept;
    const Token* expect(TokenKind kind, const std::string& errMsg);

    void synchronize();
    void error(SourceSpan span, std::string message);

    // Parsing methods
    std::unique_ptr<ImportDecl> parseImport();
    std::unique_ptr<FunctionDecl> parseFunction();
    std::unique_ptr<FunctionDecl> parseExternFunction();
    StructAttribute parseStructAttributes();
    std::unique_ptr<StructDecl> parseStruct(StructAttribute attr = {});
    std::unique_ptr<EnumDecl> parseEnum();
    std::unique_ptr<InterfaceDecl> parseInterface();
    std::unique_ptr<ClassDecl> parseClass();
    std::vector<Parameter> parseParameterList();
    std::vector<std::string> parseTypeParameterList();
    std::vector<std::string> parseTypeArgumentList();
    bool isGenericCallOrStructInit() const noexcept;
    std::string parseType();

    std::unique_ptr<BlockStmt> parseBlock();
    std::unique_ptr<StmtAST> parseStatement();
    std::unique_ptr<VarDeclStmt> parseVarDecl();
    std::unique_ptr<ReturnStmt> parseReturn();
    std::unique_ptr<IfStmt> parseIf();
    std::unique_ptr<WhileStmt> parseWhile();
    std::unique_ptr<StmtAST> parseForeach();
    std::unique_ptr<EchoStmt> parseEcho();
    std::unique_ptr<TryStmt> parseTry();
    std::unique_ptr<ThrowStmt> parseThrow();
    std::unique_ptr<DeferStmt> parseDefer();
    std::unique_ptr<ExprStmt> parseExprStatement();

    // Expression parsing (precedence hierarchy)
    std::unique_ptr<ExprAST> parseExpression();
    std::unique_ptr<ExprAST> parseAssignment();
    std::unique_ptr<ExprAST> parseLogicalOr();
    std::unique_ptr<ExprAST> parseLogicalAnd();
    std::unique_ptr<ExprAST> parseBitwiseOr();
    std::unique_ptr<ExprAST> parseBitwiseXor();
    std::unique_ptr<ExprAST> parseBitwiseAnd();
    std::unique_ptr<ExprAST> parseEquality();
    std::unique_ptr<ExprAST> parseRelational();
    std::unique_ptr<ExprAST> parseShift();
    std::unique_ptr<ExprAST> parseAdditive();
    std::unique_ptr<ExprAST> parseMultiplicative();
    std::unique_ptr<ExprAST> parseCast();
    std::unique_ptr<ExprAST> parseUnary();
    std::unique_ptr<ExprAST> parsePostfix();
    std::unique_ptr<ExprAST> parsePrimary();

    uint32_t m_fileId;
    SourceManager& m_sourceManager;
    DiagnosticEmitter* m_diagnostics;
    std::shared_ptr<std::unordered_set<std::string>> m_visitedFiles;
    std::vector<Token> m_tokens;
    size_t m_current = 0;
    bool m_allowAs = true;
};

} // namespace sph

