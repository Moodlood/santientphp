#include "compiler/semantic/BorrowChecker.hpp"

namespace sph {

bool BorrowChecker::check(ProgramAST& program) {
    for (auto& fn : program.getFunctions()) {
        if (!fn->isExtern()) {
            checkFunction(*fn);
        }
    }
    return !m_emitter.hasErrors();
}

void BorrowChecker::checkFunction(FunctionDecl& fn) {
    std::unordered_map<std::string, LocalBorrowInfo> locals;

    // Register parameters as non-local (they don't live solely on caller stack)
    for (const auto& param : fn.getParams()) {
        LocalBorrowInfo info;
        info.name = param.name;
        info.isLocal = false;
        info.span = fn.getSpan();
        locals[param.name] = info;
    }

    if (fn.hasBody()) {
        checkStatement(fn.getBody(), locals);
    }
}

void BorrowChecker::checkStatement(StmtAST& stmt, std::unordered_map<std::string, LocalBorrowInfo>& locals) {
    if (auto* block = dynamic_cast<BlockStmt*>(&stmt)) {
        for (auto& s : block->getStatements()) {
            checkStatement(*s, locals);
        }
    } else if (auto* varDecl = dynamic_cast<VarDeclStmt*>(&stmt)) {
        LocalBorrowInfo info;
        info.name = varDecl->getName();
        info.isLocal = true;
        info.span = varDecl->getSpan();
        locals[varDecl->getName()] = info;

        checkExpression(varDecl->getInitializer(), locals, false);
    } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(&stmt)) {
        checkExpression(exprStmt->getExpr(), locals, false);
    } else if (auto* retStmt = dynamic_cast<ReturnStmt*>(&stmt)) {
        if (retStmt->hasValue()) {
            checkExpression(*retStmt->getValue(), locals, true);
        }
    } else if (auto* ifStmt = dynamic_cast<IfStmt*>(&stmt)) {
        checkExpression(ifStmt->getCondition(), locals, false);
        checkStatement(ifStmt->getThenBranch(), locals);
        if (ifStmt->hasElse()) {
            checkStatement(*ifStmt->getElseBranch(), locals);
        }
    } else if (auto* whileStmt = dynamic_cast<WhileStmt*>(&stmt)) {
        checkExpression(whileStmt->getCondition(), locals, false);
        checkStatement(whileStmt->getBody(), locals);
    } else if (dynamic_cast<UnsafeBlockStmt*>(&stmt)) {
        // Inside unsafe blocks, low-level memory and pointer operations are unrestricted
        return;
    }
}

void BorrowChecker::checkExpression(ExprAST& expr, std::unordered_map<std::string, LocalBorrowInfo>& locals, bool inReturn) {
    if (auto* assign = dynamic_cast<AssignExpr*>(&expr)) {
        if (auto* varTarget = dynamic_cast<VariableExpr*>(&assign->getTarget())) {
            auto it = locals.find(varTarget->getName());
            if (it != locals.end()) {
                if (it->second.sharedBorrows > 0 || it->second.mutBorrows > 0) {
                    m_emitter.emit(Diagnostic::makeError(assign->getSpan(), "cannot assign to '" + varTarget->getName() + "' because it is borrowed"));
                }
            }
        }
        checkExpression(assign->getValue(), locals, false);
    } else if (auto* unary = dynamic_cast<UnaryExpr*>(&expr)) {
        if (unary->getOp() == UnaryOp::AddressOf) {
            if (auto* varOperand = dynamic_cast<VariableExpr*>(&unary->getOperand())) {
                auto it = locals.find(varOperand->getName());
                if (it != locals.end()) {
                    if (inReturn && it->second.isLocal) {
                        m_emitter.emit(Diagnostic::makeError(unary->getSpan(), "cannot return reference to local variable '" + varOperand->getName() + "'"));
                    }
                    if (it->second.isMoved) {
                        m_emitter.emit(Diagnostic::makeError(unary->getSpan(), "borrow of moved value '" + varOperand->getName() + "'"));
                    }
                    it->second.sharedBorrows++;
                }
            }
        }
    } else if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
        for (auto& arg : call->getArgs()) {
            checkExpression(*arg, locals, false);
        }
    } else if (auto* binary = dynamic_cast<BinaryExpr*>(&expr)) {
        checkExpression(binary->getLhs(), locals, false);
        checkExpression(binary->getRhs(), locals, false);
    }
}

} // namespace sph
