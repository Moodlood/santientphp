#pragma once

#include "compiler/ast/DeclAST.hpp"
#include "compiler/ast/ExprAST.hpp"
#include "compiler/ast/StmtAST.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace sph {

class BorrowChecker {
public:
    explicit BorrowChecker(DiagnosticEmitter& emitter)
        : m_emitter(emitter) {}

    bool check(ProgramAST& program);

private:
    struct LocalBorrowInfo {
        std::string name;
        bool isLocal = true;
        int sharedBorrows = 0;
        int mutBorrows = 0;
        bool isMoved = false;
        SourceSpan span;
    };

    void checkFunction(FunctionDecl& fn);
    void checkStatement(StmtAST& stmt, std::unordered_map<std::string, LocalBorrowInfo>& locals);
    void checkExpression(ExprAST& expr, std::unordered_map<std::string, LocalBorrowInfo>& locals, bool inReturn = false);

    DiagnosticEmitter& m_emitter;
};

} // namespace sph
