#pragma once

#include "IRModule.hpp"
#include "compiler/ast/ASTVisitor.hpp"
#include "compiler/ast/DeclAST.hpp"
#include "compiler/ast/ExprAST.hpp"
#include "compiler/ast/StmtAST.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sph {

class SourceManager;

class IRGenerator : public ASTVisitor {
public:
    IRGenerator() = default;

    std::unique_ptr<IRModule> generate(ProgramAST& program, const std::string& moduleName = "main", const SourceManager* sm = nullptr);

    [[nodiscard]] const SourceLocation& getCurrentLoc() const noexcept { return m_currentLoc; }
    void setCurrentLoc(SourceLocation loc) noexcept { m_currentLoc = loc; }

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
    std::shared_ptr<Type> resolveType(const std::string& name) const;
    std::string nextTemp();
    void pushScope();
    void popScope();
    void setVariable(const std::string& name, std::shared_ptr<IRValue> slot);
    [[nodiscard]] std::shared_ptr<IRValue> getVariable(const std::string& name) const;
    std::shared_ptr<IRValue> coerceToInterface(std::shared_ptr<IRValue> val, const std::shared_ptr<Type>& targetType);
    std::shared_ptr<IRValue> boxToDynamic(std::shared_ptr<IRValue> val);
    std::shared_ptr<IRValue> emitArenaAlloc(std::shared_ptr<IRValue> arenaVal, ExprAST& valueExpr);
    void emitInst(std::shared_ptr<IRInstruction> inst);

    std::unique_ptr<IRModule> m_module;
    IRFunction* m_currentFunction = nullptr;
    IRBasicBlock* m_currentBlock = nullptr;
    std::shared_ptr<IRValue> m_lastValue;
    size_t m_tempCounter = 0;
    SourceLocation m_currentLoc;

    std::unordered_map<std::string, std::shared_ptr<Type>> m_types;
    std::unordered_map<std::string, std::vector<std::shared_ptr<Type>>> m_functionParamTypes;
    std::unordered_map<std::string, std::vector<bool>> m_functionParamByRef;
    std::unordered_map<std::string, std::unordered_map<std::string, int64_t>> m_enumVariants;
    std::vector<std::unordered_map<std::string, std::shared_ptr<IRValue>>> m_variableScopes;
    std::vector<std::unordered_set<std::string>> m_activeBoxes;
    std::vector<std::shared_ptr<IRValue>> m_activeExceptionFrames;
    std::unordered_set<std::string> m_byRefVars;
    std::unordered_set<std::string> m_asyncFunctions;

    struct LoopContext {
        IRBasicBlock* continueBlock;
        IRBasicBlock* breakBlock;
    };
    std::vector<LoopContext> m_loopStack;
};

} // namespace sph

