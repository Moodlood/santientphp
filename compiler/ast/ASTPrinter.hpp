#pragma once

#include "ASTVisitor.hpp"
#include <iosfwd>
#include <string>

namespace sph {

class ASTPrinter : public ASTVisitor {
public:
    explicit ASTPrinter(std::ostream& out);

    void print(ProgramAST& program);

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
    void indent();

    std::ostream& m_out;
    int m_indentLevel = 0;
};

} // namespace sph

