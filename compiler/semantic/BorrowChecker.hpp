#pragma once

#include "compiler/ast/DeclAST.hpp"
#include "compiler/ast/ExprAST.hpp"
#include "compiler/ast/StmtAST.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sph {

class BorrowChecker {
public:
    explicit BorrowChecker(DiagnosticEmitter& emitter)
        : m_emitter(emitter) {}

    bool check(ProgramAST& program);

private:
    struct LocalVarInfo {
        std::string name;
        bool isLocal = true;
        bool isMut = false;
        bool isMoved = false;
        SourceSpan declSpan;
    };

    struct BorrowRecord {
        size_t id = 0;
        std::string borrowedVar;
        std::string refVar; // empty if anonymous
        bool isMutable = false;
        SourceSpan span;
        size_t blockId = 0;
        size_t pointIndex = 0;
    };

    enum class PointKind {
        VarDecl,
        Assign,
        Read,
        Borrow,
        Return,
        UnsafeEnter,
        UnsafeExit
    };

    struct CFGPoint {
        PointKind kind;
        std::string targetVar;
        std::vector<std::string> usedVars;
        std::vector<BorrowRecord> borrows;
        SourceSpan span;
        bool isReturn = false;
        bool isLocalRefReturn = false;
    };

    struct CFGBlock {
        size_t id = 0;
        std::vector<CFGPoint> points;
        std::vector<size_t> successors;
        std::vector<size_t> predecessors;
        std::unordered_set<std::string> defs;
        std::unordered_set<std::string> uses;
        std::unordered_set<std::string> liveIn;
        std::unordered_set<std::string> liveOut;
    };

    void checkFunction(FunctionDecl& fn);
    void buildCFG(StmtAST& stmt, size_t& currentBlockId, std::vector<CFGBlock>& blocks,
                  std::unordered_map<std::string, LocalVarInfo>& locals,
                  std::vector<BorrowRecord>& allBorrows, size_t& nextBorrowId);
    void extractExpressions(ExprAST& expr, CFGPoint& pt,
                            std::unordered_map<std::string, LocalVarInfo>& locals,
                            std::vector<BorrowRecord>& allBorrows, size_t& nextBorrowId,
                            const std::string& boundRefVar = "");

    void computeLiveness(std::vector<CFGBlock>& blocks);
    void checkBorrowSafety(const std::vector<CFGBlock>& blocks,
                           const std::unordered_map<std::string, LocalVarInfo>& locals,
                           const std::vector<BorrowRecord>& allBorrows);

    DiagnosticEmitter& m_emitter;
};

} // namespace sph
