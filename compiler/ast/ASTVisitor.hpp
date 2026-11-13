#pragma once

namespace sph {

// Expressions
class IntegerLiteralExpr;
class FloatLiteralExpr;
class StringLiteralExpr;
class BoolLiteralExpr;
class VariableExpr;
class BinaryExpr;
class UnaryExpr;
class CallExpr;
class AssignExpr;
class StructInitExpr;
class MemberAccessExpr;
class ArrayLiteralExpr;
class IndexExpr;
class RangeExpr;
class SliceExpr;
class ScopedAccessExpr;
class ThisExpr;
class MethodCallExpr;
class CastExpr;
class RcNewExpr;
class RcCountExpr;
class BoxNewExpr;
class ArenaNewExpr;
class ArenaAllocExpr;
class ArenaResetExpr;
class ArenaDestroyExpr;
class VecNewExpr;
class NewExpr;
class AwaitExpr;

// Statements
class VarDeclStmt;
class ReturnStmt;
class IfStmt;
class WhileStmt;
class ForeachStmt;
class BreakStmt;
class ContinueStmt;
class ExprStmt;
class BlockStmt;
class UnsafeBlockStmt;
class EchoStmt;
class ThrowStmt;
class TryStmt;
class DeferStmt;

// Declarations
class ImportDecl;
class FunctionDecl;
class StructDecl;
class EnumDecl;
class InterfaceDecl;
class ClassDecl;
class ProgramAST;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(IntegerLiteralExpr& node) = 0;
    virtual void visit(FloatLiteralExpr& node) = 0;
    virtual void visit(StringLiteralExpr& node) = 0;
    virtual void visit(BoolLiteralExpr& node) = 0;
    virtual void visit(VariableExpr& node) = 0;
    virtual void visit(BinaryExpr& node) = 0;
    virtual void visit(UnaryExpr& node) = 0;
    virtual void visit(CallExpr& node) = 0;
    virtual void visit(AssignExpr& node) = 0;
    virtual void visit(StructInitExpr& node) = 0;
    virtual void visit(MemberAccessExpr& node) = 0;
    virtual void visit(ArrayLiteralExpr& node) = 0;
    virtual void visit(IndexExpr& node) = 0;
    virtual void visit(RangeExpr& node) = 0;
    virtual void visit(SliceExpr& node) = 0;
    virtual void visit(ScopedAccessExpr& node) = 0;
    virtual void visit(ThisExpr& node) = 0;
    virtual void visit(MethodCallExpr& node) = 0;
    virtual void visit(CastExpr& node) = 0;
    virtual void visit(RcNewExpr& node) = 0;
    virtual void visit(RcCountExpr& node) = 0;
    virtual void visit(BoxNewExpr& node) = 0;
    virtual void visit(ArenaNewExpr& node) = 0;
    virtual void visit(ArenaAllocExpr& node) = 0;
    virtual void visit(ArenaResetExpr& node) = 0;
    virtual void visit(ArenaDestroyExpr& node) = 0;
    virtual void visit(VecNewExpr& node) = 0;
    virtual void visit(NewExpr& node) = 0;
    virtual void visit(AwaitExpr& node) = 0;

    virtual void visit(VarDeclStmt& node) = 0;
    virtual void visit(ReturnStmt& node) = 0;
    virtual void visit(IfStmt& node) = 0;
    virtual void visit(WhileStmt& node) = 0;
    virtual void visit(ForeachStmt& node) = 0;
    virtual void visit(BreakStmt& node) = 0;
    virtual void visit(ContinueStmt& node) = 0;
    virtual void visit(ExprStmt& node) = 0;
    virtual void visit(EchoStmt& node) = 0;
    virtual void visit(ThrowStmt& node) = 0;
    virtual void visit(TryStmt& node) = 0;
    virtual void visit(DeferStmt& node) = 0;
    virtual void visit(BlockStmt& node) = 0;
    virtual void visit(UnsafeBlockStmt& node) = 0;

    virtual void visit(ImportDecl& node) = 0;
    virtual void visit(FunctionDecl& node) = 0;
    virtual void visit(StructDecl& node) = 0;
    virtual void visit(EnumDecl& node) = 0;
    virtual void visit(InterfaceDecl& node) = 0;
    virtual void visit(ClassDecl& node) = 0;
    virtual void visit(ProgramAST& node) = 0;
};

} // namespace sph

