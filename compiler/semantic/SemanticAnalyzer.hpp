#pragma once

#include "Scope.hpp"
#include "compiler/ast/ASTVisitor.hpp"
#include "compiler/ast/DeclAST.hpp"
#include "compiler/ast/ExprAST.hpp"
#include "compiler/ast/StmtAST.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/types/Type.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace sph {

class SemanticAnalyzer : public ASTVisitor {
public:
    explicit SemanticAnalyzer(DiagnosticEmitter& diagnostics);

    void setDynamicMode(bool enabled) noexcept { m_isDynamicMode = enabled; }
    [[nodiscard]] bool isDynamicMode() const noexcept { return m_isDynamicMode; }

    bool analyze(ProgramAST& program);

    void visit(IntegerLiteralExpr& node) override;
    void visit(FloatLiteralExpr& node) override;
    void visit(StringLiteralExpr& node) override;
    void visit(BoolLiteralExpr& node) override;
    void visit(VariableExpr& node) override;
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(CallExpr& node) override;
    void visit(AssignExpr& node) override;
    void visit(StructInitExpr& node) override;
    void visit(MemberAccessExpr& node) override;
    void visit(ArrayLiteralExpr& node) override;
    void visit(IndexExpr& node) override;
    void visit(RangeExpr& node) override;
    void visit(SliceExpr& node) override;
    void visit(ScopedAccessExpr& node) override;
    void visit(ThisExpr& node) override;
    void visit(MethodCallExpr& node) override;
    void visit(CastExpr& node) override;
    void visit(RcNewExpr& node) override;
    void visit(RcCountExpr& node) override;
    void visit(BoxNewExpr& node) override;
    void visit(ArenaNewExpr& node) override;
    void visit(ArenaAllocExpr& node) override;
    void visit(ArenaResetExpr& node) override;
    void visit(ArenaDestroyExpr& node) override;
    void visit(VecNewExpr& node) override;
    void visit(NewExpr& node) override;
    void visit(AwaitExpr& node) override;

    void visit(VarDeclStmt& node) override;
    void visit(ReturnStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(ForeachStmt& node) override;
    void visit(BreakStmt& node) override;
    void visit(ContinueStmt& node) override;
    void visit(ExprStmt& node) override;
    void visit(EchoStmt& node) override;
    void visit(ThrowStmt& node) override;
    void visit(TryStmt& node) override;
    void visit(BlockStmt& node) override;
    void visit(UnsafeBlockStmt& node) override;

    void visit(ImportDecl& node) override;
    void visit(FunctionDecl& node) override;
    void visit(StructDecl& node) override;
    void visit(EnumDecl& node) override;
    void visit(InterfaceDecl& node) override;
    void visit(ClassDecl& node) override;
    void visit(ProgramAST& node) override;

private:
    void error(SourceSpan span, std::string message);
    bool checkReturns(const StmtAST& stmt) const;
    std::shared_ptr<Type> resolveType(const std::string& name);
    bool isAssignable(const Type& from, const Type& to) const;

    std::string getMangledStructName(const std::string& baseName, const std::vector<std::string>& typeArgs);
    std::string getMangledFunctionName(const std::string& baseName, const std::vector<std::string>& typeArgs);
    std::shared_ptr<Type> instantiateGenericStruct(const std::string& baseName, const std::vector<std::string>& typeArgs, SourceSpan span);
    void instantiateGenericFunction(const std::string& baseName, const std::vector<std::string>& typeArgs, SourceSpan span);

    DiagnosticEmitter& m_diagnostics;
    Scope m_globalScope;
    Scope* m_currentScope = &m_globalScope;
    const FunctionDecl* m_currentFunction = nullptr;
    std::shared_ptr<Type> m_currentFunctionReturnType = nullptr;
    std::shared_ptr<Type> m_expectedType = nullptr;
    std::shared_ptr<ClassType> m_currentClass = nullptr;
    bool m_currentMethodMutatesThis = false;
    int m_unsafeDepth = 0;
    int m_loopDepth = 0;
    bool m_isDynamicMode = false;

    ProgramAST* m_program = nullptr;
    std::unordered_map<std::string, const FunctionDecl*> m_functionDecls;
    std::unordered_map<std::string, const StructDecl*> m_genericStructs;
    std::unordered_map<std::string, const FunctionDecl*> m_genericFunctions;
    std::unordered_set<std::string> m_specializedFunctions;
    std::unordered_set<std::string> m_specializedStructs;
};

} // namespace sph

