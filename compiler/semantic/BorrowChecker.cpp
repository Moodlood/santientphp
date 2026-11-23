#include "compiler/semantic/BorrowChecker.hpp"
#include <algorithm>
#include <queue>

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
    std::unordered_map<std::string, LocalVarInfo> locals;
    std::vector<BorrowRecord> allBorrows;
    size_t nextBorrowId = 1;

    // Register parameters as non-local
    for (const auto& param : fn.getParams()) {
        LocalVarInfo info;
        info.name = param.name;
        info.isLocal = false;
        info.declSpan = fn.getSpan();
        locals[param.name] = info;
    }

    if (!fn.hasBody()) {
        return;
    }

    std::vector<CFGBlock> blocks;
    blocks.emplace_back(); // Entry block 0
    blocks[0].id = 0;

    size_t currentBlockId = 0;
    buildCFG(fn.getBody(), currentBlockId, blocks, locals, allBorrows, nextBorrowId);

    // Compute CFG liveness
    computeLiveness(blocks);

    // Check borrow safety across CFG live ranges
    checkBorrowSafety(blocks, locals, allBorrows);
}

void BorrowChecker::buildCFG(StmtAST& stmt, size_t& currentBlockId, std::vector<CFGBlock>& blocks,
                             std::unordered_map<std::string, LocalVarInfo>& locals,
                             std::vector<BorrowRecord>& allBorrows, size_t& nextBorrowId) {
    if (auto* block = dynamic_cast<BlockStmt*>(&stmt)) {
        for (auto& s : block->getStatements()) {
            buildCFG(*s, currentBlockId, blocks, locals, allBorrows, nextBorrowId);
        }
    } else if (auto* varDecl = dynamic_cast<VarDeclStmt*>(&stmt)) {
        LocalVarInfo info;
        info.name = varDecl->getName();
        info.isLocal = true;
        info.isMut = varDecl->isMut();
        info.declSpan = varDecl->getSpan();
        locals[varDecl->getName()] = info;

        CFGPoint pt;
        pt.kind = PointKind::VarDecl;
        pt.targetVar = varDecl->getName();
        pt.span = varDecl->getSpan();

        extractExpressions(varDecl->getInitializer(), pt, locals, allBorrows, nextBorrowId, varDecl->getName());
        blocks[currentBlockId].points.push_back(std::move(pt));
    } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(&stmt)) {
        CFGPoint pt;
        pt.kind = PointKind::Read;
        pt.span = exprStmt->getSpan();

        if (auto* assign = dynamic_cast<AssignExpr*>(&exprStmt->getExpr())) {
            pt.kind = PointKind::Assign;
            std::string targetName;
            if (auto* varTarget = dynamic_cast<VariableExpr*>(&assign->getTarget())) {
                targetName = varTarget->getName();
                pt.targetVar = targetName;
            }
            extractExpressions(assign->getValue(), pt, locals, allBorrows, nextBorrowId, targetName);
        } else {
            extractExpressions(exprStmt->getExpr(), pt, locals, allBorrows, nextBorrowId, "");
        }
        blocks[currentBlockId].points.push_back(std::move(pt));
    } else if (auto* retStmt = dynamic_cast<ReturnStmt*>(&stmt)) {
        CFGPoint pt;
        pt.kind = PointKind::Return;
        pt.span = retStmt->getSpan();
        pt.isReturn = true;

        if (retStmt->hasValue()) {
            if (auto* un = dynamic_cast<const UnaryExpr*>(retStmt->getValue())) {
                if (un->getOp() == UnaryOp::AddressOf) {
                    if (auto* ve = dynamic_cast<const VariableExpr*>(&un->getOperand())) {
                        auto it = locals.find(ve->getName());
                        if (it != locals.end() && it->second.isLocal) {
                            pt.isLocalRefReturn = true;
                            pt.targetVar = ve->getName();
                        }
                    }
                }
            }
            extractExpressions(*retStmt->getValue(), pt, locals, allBorrows, nextBorrowId, "");
        }
        blocks[currentBlockId].points.push_back(std::move(pt));
        // Return terminates flow in current block
        size_t nextBlockId = blocks.size();
        blocks.emplace_back();
        blocks[nextBlockId].id = nextBlockId;
        currentBlockId = nextBlockId;
    } else if (auto* ifStmt = dynamic_cast<IfStmt*>(&stmt)) {
        CFGPoint condPt;
        condPt.kind = PointKind::Read;
        condPt.span = ifStmt->getCondition().getSpan();
        extractExpressions(ifStmt->getCondition(), condPt, locals, allBorrows, nextBorrowId, "");
        blocks[currentBlockId].points.push_back(std::move(condPt));

        size_t condBlockId = currentBlockId;
        size_t thenBlockId = blocks.size();
        blocks.emplace_back();
        blocks[thenBlockId].id = thenBlockId;

        blocks[condBlockId].successors.push_back(thenBlockId);
        blocks[thenBlockId].predecessors.push_back(condBlockId);

        size_t thenCur = thenBlockId;
        buildCFG(ifStmt->getThenBranch(), thenCur, blocks, locals, allBorrows, nextBorrowId);

        size_t joinBlockId = blocks.size();
        blocks.emplace_back();
        blocks[joinBlockId].id = joinBlockId;

        blocks[thenCur].successors.push_back(joinBlockId);
        blocks[joinBlockId].predecessors.push_back(thenCur);

        if (ifStmt->hasElse()) {
            size_t elseBlockId = blocks.size();
            blocks.emplace_back();
            blocks[elseBlockId].id = elseBlockId;

            blocks[condBlockId].successors.push_back(elseBlockId);
            blocks[elseBlockId].predecessors.push_back(condBlockId);

            size_t elseCur = elseBlockId;
            buildCFG(*ifStmt->getElseBranch(), elseCur, blocks, locals, allBorrows, nextBorrowId);

            blocks[elseCur].successors.push_back(joinBlockId);
            blocks[joinBlockId].predecessors.push_back(elseCur);
        } else {
            blocks[condBlockId].successors.push_back(joinBlockId);
            blocks[joinBlockId].predecessors.push_back(condBlockId);
        }

        currentBlockId = joinBlockId;
    } else if (auto* whileStmt = dynamic_cast<WhileStmt*>(&stmt)) {
        size_t headerBlockId = blocks.size();
        blocks.emplace_back();
        blocks[headerBlockId].id = headerBlockId;

        blocks[currentBlockId].successors.push_back(headerBlockId);
        blocks[headerBlockId].predecessors.push_back(currentBlockId);

        CFGPoint condPt;
        condPt.kind = PointKind::Read;
        condPt.span = whileStmt->getCondition().getSpan();
        extractExpressions(whileStmt->getCondition(), condPt, locals, allBorrows, nextBorrowId, "");
        blocks[headerBlockId].points.push_back(std::move(condPt));

        size_t bodyBlockId = blocks.size();
        blocks.emplace_back();
        blocks[bodyBlockId].id = bodyBlockId;

        blocks[headerBlockId].successors.push_back(bodyBlockId);
        blocks[bodyBlockId].predecessors.push_back(headerBlockId);

        size_t bodyCur = bodyBlockId;
        buildCFG(whileStmt->getBody(), bodyCur, blocks, locals, allBorrows, nextBorrowId);

        blocks[bodyCur].successors.push_back(headerBlockId);
        blocks[headerBlockId].predecessors.push_back(bodyCur);

        size_t exitBlockId = blocks.size();
        blocks.emplace_back();
        blocks[exitBlockId].id = exitBlockId;

        blocks[headerBlockId].successors.push_back(exitBlockId);
        blocks[exitBlockId].predecessors.push_back(headerBlockId);

        currentBlockId = exitBlockId;
    } else if (auto* ub = dynamic_cast<UnsafeBlockStmt*>(&stmt)) {
        CFGPoint enterPt;
        enterPt.kind = PointKind::UnsafeEnter;
        blocks[currentBlockId].points.push_back(enterPt);

        buildCFG(ub->getBlock(), currentBlockId, blocks, locals, allBorrows, nextBorrowId);

        CFGPoint exitPt;
        exitPt.kind = PointKind::UnsafeExit;
        blocks[currentBlockId].points.push_back(exitPt);
    }
}

void BorrowChecker::extractExpressions(ExprAST& expr, CFGPoint& pt,
                                       std::unordered_map<std::string, LocalVarInfo>& locals,
                                       std::vector<BorrowRecord>& allBorrows, size_t& nextBorrowId,
                                       const std::string& boundRefVar) {
    if (auto* var = dynamic_cast<VariableExpr*>(&expr)) {
        pt.usedVars.push_back(var->getName());
    } else if (auto* un = dynamic_cast<UnaryExpr*>(&expr)) {
        if (un->getOp() == UnaryOp::AddressOf) {
            if (auto* targetVar = dynamic_cast<VariableExpr*>(&un->getOperand())) {
                BorrowRecord record;
                record.id = nextBorrowId++;
                record.borrowedVar = targetVar->getName();
                record.refVar = boundRefVar;
                record.isMutable = un->isMut();
                // If the target variable itself is mutable and we took its address into a variable,
                // consider it mutable if not explicitly specified as immutable
                auto it = locals.find(targetVar->getName());
                if (it != locals.end() && it->second.isMut && un->isMut()) {
                    record.isMutable = true;
                }
                record.span = un->getSpan();
                pt.borrows.push_back(record);
                allBorrows.push_back(record);
            }
        }
        extractExpressions(un->getOperand(), pt, locals, allBorrows, nextBorrowId, boundRefVar);
    } else if (auto* bin = dynamic_cast<BinaryExpr*>(&expr)) {
        extractExpressions(bin->getLhs(), pt, locals, allBorrows, nextBorrowId, boundRefVar);
        extractExpressions(bin->getRhs(), pt, locals, allBorrows, nextBorrowId, boundRefVar);
    } else if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
        for (auto& arg : call->getArgs()) {
            extractExpressions(*arg, pt, locals, allBorrows, nextBorrowId, "");
        }
    } else if (auto* assign = dynamic_cast<AssignExpr*>(&expr)) {
        extractExpressions(assign->getTarget(), pt, locals, allBorrows, nextBorrowId, "");
        extractExpressions(assign->getValue(), pt, locals, allBorrows, nextBorrowId, "");
    }
}

void BorrowChecker::computeLiveness(std::vector<CFGBlock>& blocks) {
    // 1. Identify defs and uses per block
    for (auto& block : blocks) {
        for (const auto& pt : block.points) {
            for (const auto& u : pt.usedVars) {
                if (block.defs.find(u) == block.defs.end()) {
                    block.uses.insert(u);
                }
            }
            if (!pt.targetVar.empty()) {
                block.defs.insert(pt.targetVar);
            }
        }
    }

    // 2. Iterative backward dataflow analysis
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
            auto& block = *it;

            std::unordered_set<std::string> newLiveOut;
            for (size_t succId : block.successors) {
                if (succId < blocks.size()) {
                    newLiveOut.insert(blocks[succId].liveIn.begin(), blocks[succId].liveIn.end());
                }
            }

            std::unordered_set<std::string> newLiveIn = block.uses;
            for (const auto& v : newLiveOut) {
                if (block.defs.find(v) == block.defs.end()) {
                    newLiveIn.insert(v);
                }
            }

            if (newLiveIn != block.liveIn || newLiveOut != block.liveOut) {
                block.liveIn = std::move(newLiveIn);
                block.liveOut = std::move(newLiveOut);
                changed = true;
            }
        }
    }
}

void BorrowChecker::checkBorrowSafety(const std::vector<CFGBlock>& blocks,
                                      const std::unordered_map<std::string, LocalVarInfo>& locals,
                                      const std::vector<BorrowRecord>& allBorrows) {
    bool inUnsafe = false;

    // Determine downstream usage of each refVar
    std::unordered_map<std::string, bool> refVarHasDownstreamUse;
    for (const auto& block : blocks) {
        for (const auto& pt : block.points) {
            for (const auto& u : pt.usedVars) {
                refVarHasDownstreamUse[u] = true;
            }
        }
    }

    // Process blocks in topological order
    for (const auto& block : blocks) {
        std::unordered_set<std::string> currentLive = block.liveOut;

        // Step backward through points to compute per-point liveness
        std::vector<std::unordered_set<std::string>> liveAfterPoint(block.points.size());
        for (int i = static_cast<int>(block.points.size()) - 1; i >= 0; --i) {
            liveAfterPoint[i] = currentLive;
            const auto& pt = block.points[i];
            if (!pt.targetVar.empty()) {
                currentLive.erase(pt.targetVar);
            }
            for (const auto& u : pt.usedVars) {
                currentLive.insert(u);
            }
        }

        // Now step forward through points checking borrow safety
        for (size_t pIdx = 0; pIdx < block.points.size(); ++pIdx) {
            const auto& pt = block.points[pIdx];

            if (pt.kind == PointKind::UnsafeEnter) {
                inUnsafe = true;
                continue;
            }
            if (pt.kind == PointKind::UnsafeExit) {
                inUnsafe = false;
                continue;
            }
            if (inUnsafe) {
                continue;
            }

            // Check return of reference to local variable
            if (pt.isLocalRefReturn) {
                m_emitter.emit(Diagnostic::makeError(pt.span, "cannot return reference to local variable '" + pt.targetVar + "'"));
            }

            // Check new borrows created at this point
            for (const auto& borrow : pt.borrows) {
                auto itLoc = locals.find(borrow.borrowedVar);
                if (itLoc != locals.end() && itLoc->second.isMoved) {
                    m_emitter.emit(Diagnostic::makeError(borrow.span, "borrow of moved value '" + borrow.borrowedVar + "'"));
                }

                // Check conflict with other active borrows of this variable
                for (const auto& existing : allBorrows) {
                    if (existing.id >= borrow.id || existing.borrowedVar != borrow.borrowedVar) {
                        continue;
                    }

                    bool existingIsLive = false;
                    if (!existing.refVar.empty()) {
                        existingIsLive = liveAfterPoint[pIdx].count(existing.refVar) > 0;
                    }

                    if (!existingIsLive) {
                        continue;
                    }

                    if (borrow.isMutable) {
                        if (existing.isMutable) {
                            Diagnostic diag = Diagnostic::makeError(borrow.span,
                                "cannot borrow '" + borrow.borrowedVar + "' as mutable more than once at a time");
                            diag.addRelatedSpan(existing.span, "first mutable borrow occurs here");
                            m_emitter.emit(diag);
                        } else {
                            Diagnostic diag = Diagnostic::makeError(borrow.span,
                                "cannot borrow '" + borrow.borrowedVar + "' as mutable because it is also borrowed as immutable");
                            diag.addRelatedSpan(existing.span, "immutable borrow occurs here");
                            m_emitter.emit(diag);
                        }
                    } else if (existing.isMutable) {
                        Diagnostic diag = Diagnostic::makeError(borrow.span,
                            "cannot borrow '" + borrow.borrowedVar + "' as immutable because it is also borrowed as mutable");
                        diag.addRelatedSpan(existing.span, "mutable borrow occurs here");
                        m_emitter.emit(diag);
                    }
                }
            }

            // Check mutation / assignment to a borrowed variable
            if (pt.kind == PointKind::Assign && !pt.targetVar.empty()) {
                for (const auto& existing : allBorrows) {
                    if (existing.borrowedVar != pt.targetVar) {
                        continue;
                    }

                    bool existingIsLive = false;
                    if (!existing.refVar.empty()) {
                        if (refVarHasDownstreamUse[existing.refVar]) {
                            existingIsLive = liveAfterPoint[pIdx].count(existing.refVar) > 0;
                        } else {
                            existingIsLive = true;
                        }
                    }

                    if (existingIsLive) {
                        Diagnostic diag = Diagnostic::makeError(pt.span,
                            "cannot assign to '" + pt.targetVar + "' because it is borrowed");
                        diag.addRelatedSpan(existing.span, "borrow of '" + pt.targetVar + "' occurs here");
                        m_emitter.emit(diag);
                        break;
                    }
                }
            }
        }
    }
}

} // namespace sph
