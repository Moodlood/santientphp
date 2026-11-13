#pragma once

#include "ASTNode.hpp"
#include "ASTVisitor.hpp"
#include "ExprAST.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sph {

class StmtAST : public ASTNode {
public:
    explicit StmtAST(SourceSpan span) : ASTNode(span) {}
};

class BlockStmt : public StmtAST {
public:
    BlockStmt(std::vector<std::unique_ptr<StmtAST>> stmts, SourceSpan span)
        : StmtAST(span), m_statements(std::move(stmts)) {}

    [[nodiscard]] const std::vector<std::unique_ptr<StmtAST>>& getStatements() const noexcept { return m_statements; }
    [[nodiscard]] std::vector<std::unique_ptr<StmtAST>>& getStatements() noexcept { return m_statements; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::vector<std::unique_ptr<StmtAST>> m_statements;
};

class VarDeclStmt : public StmtAST {
public:
    VarDeclStmt(bool isMut, std::string name, std::optional<std::string> typeAnnotation,
                std::unique_ptr<ExprAST> initializer, SourceSpan span)
        : StmtAST(span), m_isMut(isMut), m_name(std::move(name)),
          m_typeAnnotation(std::move(typeAnnotation)), m_initializer(std::move(initializer)) {}

    [[nodiscard]] bool isMut() const noexcept { return m_isMut; }
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] const std::optional<std::string>& getTypeAnnotation() const noexcept { return m_typeAnnotation; }
    [[nodiscard]] const ExprAST& getInitializer() const noexcept { return *m_initializer; }
    [[nodiscard]] ExprAST& getInitializer() noexcept { return *m_initializer; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    bool m_isMut;
    std::string m_name;
    std::optional<std::string> m_typeAnnotation;
    std::unique_ptr<ExprAST> m_initializer;
};

class ReturnStmt : public StmtAST {
public:
    ReturnStmt(std::unique_ptr<ExprAST> value, SourceSpan span)
        : StmtAST(span), m_value(std::move(value)) {}

    [[nodiscard]] bool hasValue() const noexcept { return m_value != nullptr; }
    [[nodiscard]] const ExprAST* getValue() const noexcept { return m_value.get(); }
    [[nodiscard]] ExprAST* getValue() noexcept { return m_value.get(); }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_value;
};

class IfStmt : public StmtAST {
public:
    IfStmt(std::unique_ptr<ExprAST> condition, std::unique_ptr<BlockStmt> thenBranch,
           std::unique_ptr<StmtAST> elseBranch, SourceSpan span)
        : StmtAST(span), m_condition(std::move(condition)), m_thenBranch(std::move(thenBranch)),
          m_elseBranch(std::move(elseBranch)) {}

    [[nodiscard]] const ExprAST& getCondition() const noexcept { return *m_condition; }
    [[nodiscard]] ExprAST& getCondition() noexcept { return *m_condition; }
    [[nodiscard]] const BlockStmt& getThenBranch() const noexcept { return *m_thenBranch; }
    [[nodiscard]] BlockStmt& getThenBranch() noexcept { return *m_thenBranch; }
    [[nodiscard]] bool hasElse() const noexcept { return m_elseBranch != nullptr; }
    [[nodiscard]] const StmtAST* getElseBranch() const noexcept { return m_elseBranch.get(); }
    [[nodiscard]] StmtAST* getElseBranch() noexcept { return m_elseBranch.get(); }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_condition;
    std::unique_ptr<BlockStmt> m_thenBranch;
    std::unique_ptr<StmtAST> m_elseBranch; // Can be BlockStmt or nested IfStmt
};

class WhileStmt : public StmtAST {
public:
    WhileStmt(std::unique_ptr<ExprAST> condition, std::unique_ptr<BlockStmt> body, SourceSpan span)
        : StmtAST(span), m_condition(std::move(condition)), m_body(std::move(body)) {}

    [[nodiscard]] const ExprAST& getCondition() const noexcept { return *m_condition; }
    [[nodiscard]] ExprAST& getCondition() noexcept { return *m_condition; }
    [[nodiscard]] const BlockStmt& getBody() const noexcept { return *m_body; }
    [[nodiscard]] BlockStmt& getBody() noexcept { return *m_body; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_condition;
    std::unique_ptr<BlockStmt> m_body;
};

class ForeachStmt : public StmtAST {
public:
    ForeachStmt(std::unique_ptr<ExprAST> collection,
                std::optional<std::string> keyVar,
                std::string valVar,
                std::unique_ptr<BlockStmt> body,
                SourceSpan span)
        : StmtAST(span), m_collection(std::move(collection)),
          m_keyVar(std::move(keyVar)), m_valVar(std::move(valVar)),
          m_body(std::move(body)) {}

    [[nodiscard]] const ExprAST& getCollection() const noexcept { return *m_collection; }
    [[nodiscard]] ExprAST& getCollection() noexcept { return *m_collection; }
    [[nodiscard]] bool hasKeyVar() const noexcept { return m_keyVar.has_value(); }
    [[nodiscard]] const std::string& getKeyVar() const noexcept { return *m_keyVar; }
    [[nodiscard]] const std::string& getValVar() const noexcept { return m_valVar; }
    [[nodiscard]] const BlockStmt& getBody() const noexcept { return *m_body; }
    [[nodiscard]] BlockStmt& getBody() noexcept { return *m_body; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_collection;
    std::optional<std::string> m_keyVar;
    std::string m_valVar;
    std::unique_ptr<BlockStmt> m_body;
};

class BreakStmt : public StmtAST {
public:
    explicit BreakStmt(SourceSpan span) : StmtAST(span) {}
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class ContinueStmt : public StmtAST {
public:
    explicit ContinueStmt(SourceSpan span) : StmtAST(span) {}
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class ExprStmt : public StmtAST {
public:
    ExprStmt(std::unique_ptr<ExprAST> expr, SourceSpan span)
        : StmtAST(span), m_expr(std::move(expr)) {}

    [[nodiscard]] const ExprAST& getExpr() const noexcept { return *m_expr; }
    [[nodiscard]] ExprAST& getExpr() noexcept { return *m_expr; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_expr;
};

class UnsafeBlockStmt : public StmtAST {
public:
    UnsafeBlockStmt(std::unique_ptr<BlockStmt> block, SourceSpan span)
        : StmtAST(span), m_block(std::move(block)) {}

    [[nodiscard]] const BlockStmt& getBlock() const noexcept { return *m_block; }
    [[nodiscard]] BlockStmt& getBlock() noexcept { return *m_block; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<BlockStmt> m_block;
};

class EchoStmt : public StmtAST {
public:
    EchoStmt(std::vector<std::unique_ptr<ExprAST>> expressions, SourceSpan span)
        : StmtAST(span), m_expressions(std::move(expressions)) {}

    [[nodiscard]] const std::vector<std::unique_ptr<ExprAST>>& getExpressions() const noexcept { return m_expressions; }
    [[nodiscard]] std::vector<std::unique_ptr<ExprAST>>& getExpressions() noexcept { return m_expressions; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::vector<std::unique_ptr<ExprAST>> m_expressions;
};

class ThrowStmt : public StmtAST {
public:
    ThrowStmt(std::unique_ptr<ExprAST> expr, SourceSpan span)
        : StmtAST(span), m_expr(std::move(expr)) {}

    [[nodiscard]] const ExprAST& getExpr() const noexcept { return *m_expr; }
    [[nodiscard]] ExprAST& getExpr() noexcept { return *m_expr; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_expr;
};

struct CatchClause {
    std::string type;
    std::string varName;
    std::unique_ptr<BlockStmt> body;
    SourceSpan span;
};

class TryStmt : public StmtAST {
public:
    TryStmt(std::unique_ptr<BlockStmt> tryBlock,
            std::vector<CatchClause> catchClauses,
            std::unique_ptr<BlockStmt> finallyBlock,
            SourceSpan span)
        : StmtAST(span), m_tryBlock(std::move(tryBlock)),
          m_catchClauses(std::move(catchClauses)),
          m_finallyBlock(std::move(finallyBlock)) {}

    [[nodiscard]] const BlockStmt& getTryBlock() const noexcept { return *m_tryBlock; }
    [[nodiscard]] BlockStmt& getTryBlock() noexcept { return *m_tryBlock; }

    [[nodiscard]] const std::vector<CatchClause>& getCatchClauses() const noexcept { return m_catchClauses; }
    [[nodiscard]] std::vector<CatchClause>& getCatchClauses() noexcept { return m_catchClauses; }

    [[nodiscard]] bool hasFinally() const noexcept { return m_finallyBlock != nullptr; }
    [[nodiscard]] const BlockStmt* getFinallyBlock() const noexcept { return m_finallyBlock.get(); }
    [[nodiscard]] BlockStmt* getFinallyBlock() noexcept { return m_finallyBlock.get(); }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<BlockStmt> m_tryBlock;
    std::vector<CatchClause> m_catchClauses;
    std::unique_ptr<BlockStmt> m_finallyBlock;
};

class DeferStmt : public StmtAST {
public:
    DeferStmt(std::unique_ptr<StmtAST> statement, SourceSpan span)
        : StmtAST(span), m_statement(std::move(statement)) {}

    [[nodiscard]] const StmtAST& getStatement() const noexcept { return *m_statement; }
    [[nodiscard]] StmtAST& getStatement() noexcept { return *m_statement; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<StmtAST> m_statement;
};

} // namespace sph

