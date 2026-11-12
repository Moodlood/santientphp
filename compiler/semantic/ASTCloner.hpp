#pragma once

#include "compiler/ast/DeclAST.hpp"
#include "compiler/ast/ExprAST.hpp"
#include "compiler/ast/StmtAST.hpp"
#include <cctype>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sph {

class ASTCloner {
public:
    explicit ASTCloner(const std::unordered_map<std::string, std::string>& typeSubsts)
        : m_typeSubsts(typeSubsts) {}

    std::string substituteType(const std::string& typeStr) const {
        if (m_typeSubsts.empty() || typeStr.empty()) return typeStr;
        std::string result;
        size_t i = 0;
        while (i < typeStr.size()) {
            if (std::isalpha(static_cast<unsigned char>(typeStr[i])) || typeStr[i] == '_') {
                size_t start = i;
                while (i < typeStr.size() && (std::isalnum(static_cast<unsigned char>(typeStr[i])) || typeStr[i] == '_')) {
                    i++;
                }
                std::string ident = typeStr.substr(start, i - start);
                auto it = m_typeSubsts.find(ident);
                if (it != m_typeSubsts.end()) {
                    result += it->second;
                } else {
                    result += ident;
                }
            } else {
                result += typeStr[i++];
            }
        }
        return result;
    }

    std::unique_ptr<ExprAST> cloneExpr(const ExprAST& expr) const {
        if (const auto* e = dynamic_cast<const IntegerLiteralExpr*>(&expr)) {
            return std::make_unique<IntegerLiteralExpr>(e->getValue(), e->getRaw(), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const FloatLiteralExpr*>(&expr)) {
            return std::make_unique<FloatLiteralExpr>(e->getValue(), e->getRaw(), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const StringLiteralExpr*>(&expr)) {
            return std::make_unique<StringLiteralExpr>(e->getValue(), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const BoolLiteralExpr*>(&expr)) {
            return std::make_unique<BoolLiteralExpr>(e->getValue(), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const VariableExpr*>(&expr)) {
            return std::make_unique<VariableExpr>(e->getName(), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const BinaryExpr*>(&expr)) {
            return std::make_unique<BinaryExpr>(e->getOp(), cloneExpr(e->getLhs()), cloneExpr(e->getRhs()), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const UnaryExpr*>(&expr)) {
            return std::make_unique<UnaryExpr>(e->getOp(), cloneExpr(e->getOperand()), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const CallExpr*>(&expr)) {
            std::vector<std::string> newTypeArgs;
            for (const auto& t : e->getTypeArgs()) {
                newTypeArgs.push_back(substituteType(t));
            }
            std::vector<std::unique_ptr<ExprAST>> newArgs;
            for (const auto& a : e->getArgs()) {
                newArgs.push_back(cloneExpr(*a));
            }
            return std::make_unique<CallExpr>(e->getCallee(), std::move(newTypeArgs), std::move(newArgs), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const StructInitExpr*>(&expr)) {
            std::vector<std::string> newTypeArgs;
            for (const auto& t : e->getTypeArgs()) {
                newTypeArgs.push_back(substituteType(t));
            }
            std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> newFields;
            for (const auto& f : e->getFields()) {
                newFields.emplace_back(f.first, cloneExpr(*f.second));
            }
            return std::make_unique<StructInitExpr>(e->getStructName(), std::move(newTypeArgs), std::move(newFields), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const AssignExpr*>(&expr)) {
            return std::make_unique<AssignExpr>(cloneExpr(e->getTarget()), cloneExpr(e->getValue()), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const MemberAccessExpr*>(&expr)) {
            return std::make_unique<MemberAccessExpr>(cloneExpr(e->getBase()), e->getMember(), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
            std::vector<ArrayLiteralExpr::Element> newElems;
            for (const auto& elem : e->getElements()) {
                auto newKey = elem.key ? cloneExpr(*elem.key) : nullptr;
                auto newVal = cloneExpr(*elem.value);
                newElems.push_back({std::move(newKey), std::move(newVal)});
            }
            return std::make_unique<ArrayLiteralExpr>(std::move(newElems), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const IndexExpr*>(&expr)) {
            auto newIndex = e->hasIndex() ? cloneExpr(e->getIndex()) : nullptr;
            return std::make_unique<IndexExpr>(cloneExpr(e->getBase()), std::move(newIndex), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const RangeExpr*>(&expr)) {
            auto newStart = e->hasStart() ? cloneExpr(*e->getStart()) : nullptr;
            auto newEnd = e->hasEnd() ? cloneExpr(*e->getEnd()) : nullptr;
            return std::make_unique<RangeExpr>(std::move(newStart), std::move(newEnd), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const SliceExpr*>(&expr)) {
            auto newRange = std::unique_ptr<RangeExpr>(static_cast<RangeExpr*>(cloneExpr(e->getRange()).release()));
            return std::make_unique<SliceExpr>(cloneExpr(e->getBase()), std::move(newRange), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const ScopedAccessExpr*>(&expr)) {
            return std::make_unique<ScopedAccessExpr>(e->getScopeName(), e->getMemberName(), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const ThisExpr*>(&expr)) {
            return std::make_unique<ThisExpr>(e->getSpan());
        }
        if (const auto* e = dynamic_cast<const MethodCallExpr*>(&expr)) {
            std::vector<std::unique_ptr<ExprAST>> newArgs;
            for (const auto& a : e->getArgs()) {
                newArgs.push_back(cloneExpr(*a));
            }
            return std::make_unique<MethodCallExpr>(cloneExpr(e->getReceiver()), e->getMethodName(), std::move(newArgs), e->getSpan());
        }
        if (const auto* e = dynamic_cast<const CastExpr*>(&expr)) {
            return std::make_unique<CastExpr>(cloneExpr(e->getExpr()), substituteType(e->getTargetType()), e->getSpan());
        }
        return nullptr;
    }

    std::unique_ptr<StmtAST> cloneStmt(const StmtAST& stmt) const {
        if (const auto* s = dynamic_cast<const BlockStmt*>(&stmt)) {
            std::vector<std::unique_ptr<StmtAST>> newStmts;
            for (const auto& inner : s->getStatements()) {
                newStmts.push_back(cloneStmt(*inner));
            }
            return std::make_unique<BlockStmt>(std::move(newStmts), s->getSpan());
        }
        if (const auto* s = dynamic_cast<const VarDeclStmt*>(&stmt)) {
            std::optional<std::string> newTypeAnn = std::nullopt;
            if (s->getTypeAnnotation()) {
                newTypeAnn = substituteType(*s->getTypeAnnotation());
            }
            return std::make_unique<VarDeclStmt>(s->isMut(), s->getName(), std::move(newTypeAnn),
                                                 cloneExpr(s->getInitializer()), s->getSpan());
        }
        if (const auto* s = dynamic_cast<const ReturnStmt*>(&stmt)) {
            auto newRet = s->hasValue() ? cloneExpr(*s->getValue()) : nullptr;
            return std::make_unique<ReturnStmt>(std::move(newRet), s->getSpan());
        }
        if (const auto* s = dynamic_cast<const IfStmt*>(&stmt)) {
            auto newThen = std::unique_ptr<BlockStmt>(static_cast<BlockStmt*>(cloneStmt(s->getThenBranch()).release()));
            auto newElse = s->hasElse() ? cloneStmt(*s->getElseBranch()) : nullptr;
            return std::make_unique<IfStmt>(cloneExpr(s->getCondition()), std::move(newThen), std::move(newElse), s->getSpan());
        }
        if (const auto* s = dynamic_cast<const WhileStmt*>(&stmt)) {
            auto newBody = std::unique_ptr<BlockStmt>(static_cast<BlockStmt*>(cloneStmt(s->getBody()).release()));
            return std::make_unique<WhileStmt>(cloneExpr(s->getCondition()), std::move(newBody), s->getSpan());
        }
        if (const auto* s = dynamic_cast<const ForeachStmt*>(&stmt)) {
            auto newBody = std::unique_ptr<BlockStmt>(static_cast<BlockStmt*>(cloneStmt(s->getBody()).release()));
            std::optional<std::string> keyVar = s->hasKeyVar() ? std::optional<std::string>(s->getKeyVar()) : std::nullopt;
            return std::make_unique<ForeachStmt>(cloneExpr(s->getCollection()), keyVar, s->getValVar(), std::move(newBody), s->getSpan());
        }
        if (const auto* s = dynamic_cast<const BreakStmt*>(&stmt)) {
            return std::make_unique<BreakStmt>(s->getSpan());
        }
        if (const auto* s = dynamic_cast<const ContinueStmt*>(&stmt)) {
            return std::make_unique<ContinueStmt>(s->getSpan());
        }
        if (const auto* s = dynamic_cast<const ExprStmt*>(&stmt)) {
            return std::make_unique<ExprStmt>(cloneExpr(s->getExpr()), s->getSpan());
        }
        if (const auto* s = dynamic_cast<const UnsafeBlockStmt*>(&stmt)) {
            auto newBlock = std::unique_ptr<BlockStmt>(static_cast<BlockStmt*>(cloneStmt(s->getBlock()).release()));
            return std::make_unique<UnsafeBlockStmt>(std::move(newBlock), s->getSpan());
        }
        if (const auto* s = dynamic_cast<const EchoStmt*>(&stmt)) {
            std::vector<std::unique_ptr<ExprAST>> newExprs;
            for (const auto& e : s->getExpressions()) {
                newExprs.push_back(cloneExpr(*e));
            }
            return std::make_unique<EchoStmt>(std::move(newExprs), s->getSpan());
        }
        if (const auto* s = dynamic_cast<const ThrowStmt*>(&stmt)) {
            return std::make_unique<ThrowStmt>(cloneExpr(s->getExpr()), s->getSpan());
        }
        if (const auto* s = dynamic_cast<const TryStmt*>(&stmt)) {
            auto newTry = std::unique_ptr<BlockStmt>(static_cast<BlockStmt*>(cloneStmt(s->getTryBlock()).release()));
            std::vector<CatchClause> newCatches;
            for (const auto& c : s->getCatchClauses()) {
                auto newCatchBody = std::unique_ptr<BlockStmt>(static_cast<BlockStmt*>(cloneStmt(*c.body).release()));
                newCatches.push_back(CatchClause{substituteType(c.type), c.varName, std::move(newCatchBody), c.span});
            }
            std::unique_ptr<BlockStmt> newFinally = nullptr;
            if (s->hasFinally()) {
                newFinally = std::unique_ptr<BlockStmt>(static_cast<BlockStmt*>(cloneStmt(*s->getFinallyBlock()).release()));
            }
            return std::make_unique<TryStmt>(std::move(newTry), std::move(newCatches), std::move(newFinally), s->getSpan());
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, std::string> m_typeSubsts;
};

} // namespace sph
