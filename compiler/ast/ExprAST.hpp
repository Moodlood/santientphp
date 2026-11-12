#pragma once

#include "ASTNode.hpp"
#include "ASTVisitor.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sph {

class Type;

class ExprAST : public ASTNode {
public:
    explicit ExprAST(SourceSpan span) : ASTNode(span) {}

    [[nodiscard]] const std::shared_ptr<Type>& getInferredType() const noexcept { return m_inferredType; }
    void setInferredType(std::shared_ptr<Type> type) noexcept { m_inferredType = std::move(type); }

private:
    std::shared_ptr<Type> m_inferredType;
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    ShiftLeft,
    ShiftRight,
    LogicalAnd,
    LogicalOr,
    Concat
};

[[nodiscard]] inline const char* binaryOpToString(BinaryOp op) noexcept {
    switch (op) {
        case BinaryOp::Concat:       return ".";
        case BinaryOp::Add:          return "+";
        case BinaryOp::Sub:          return "-";
        case BinaryOp::Mul:          return "*";
        case BinaryOp::Div:          return "/";
        case BinaryOp::Mod:          return "%";
        case BinaryOp::Equal:        return "==";
        case BinaryOp::NotEqual:     return "!=";
        case BinaryOp::Less:         return "<";
        case BinaryOp::LessEqual:    return "<=";
        case BinaryOp::Greater:      return ">";
        case BinaryOp::GreaterEqual: return ">=";
        case BinaryOp::BitwiseAnd:   return "&";
        case BinaryOp::BitwiseOr:    return "|";
        case BinaryOp::BitwiseXor:   return "^";
        case BinaryOp::ShiftLeft:    return "<<";
        case BinaryOp::ShiftRight:   return ">>";
        case BinaryOp::LogicalAnd:   return "&&";
        case BinaryOp::LogicalOr:    return "||";
    }
    return "?";
}

enum class UnaryOp {
    Negate,
    LogicalNot,
    BitwiseNot,
    AddressOf,
    Deref
};

[[nodiscard]] inline const char* unaryOpToString(UnaryOp op) noexcept {
    switch (op) {
        case UnaryOp::Negate:     return "-";
        case UnaryOp::LogicalNot: return "!";
        case UnaryOp::BitwiseNot: return "~";
        case UnaryOp::AddressOf:  return "&";
        case UnaryOp::Deref:      return "*";
    }
    return "?";
}

class IntegerLiteralExpr : public ExprAST {
public:
    IntegerLiteralExpr(int64_t val, std::string raw, SourceSpan span)
        : ExprAST(span), m_value(val), m_raw(std::move(raw)) {}

    [[nodiscard]] int64_t getValue() const noexcept { return m_value; }
    [[nodiscard]] const std::string& getRaw() const noexcept { return m_raw; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    int64_t m_value;
    std::string m_raw;
};

class FloatLiteralExpr : public ExprAST {
public:
    FloatLiteralExpr(double val, std::string raw, SourceSpan span)
        : ExprAST(span), m_value(val), m_raw(std::move(raw)) {}

    [[nodiscard]] double getValue() const noexcept { return m_value; }
    [[nodiscard]] const std::string& getRaw() const noexcept { return m_raw; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    double m_value;
    std::string m_raw;
};

class StringLiteralExpr : public ExprAST {
public:
    StringLiteralExpr(std::string val, SourceSpan span)
        : ExprAST(span), m_value(std::move(val)) {}

    [[nodiscard]] const std::string& getValue() const noexcept { return m_value; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_value;
};

class BoolLiteralExpr : public ExprAST {
public:
    BoolLiteralExpr(bool val, SourceSpan span)
        : ExprAST(span), m_value(val) {}

    [[nodiscard]] bool getValue() const noexcept { return m_value; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    bool m_value;
};

class VariableExpr : public ExprAST {
public:
    VariableExpr(std::string name, SourceSpan span)
        : ExprAST(span), m_name(std::move(name)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_name;
};

class BinaryExpr : public ExprAST {
public:
    BinaryExpr(BinaryOp op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs, SourceSpan span)
        : ExprAST(span), m_op(op), m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {}

    [[nodiscard]] BinaryOp getOp() const noexcept { return m_op; }
    [[nodiscard]] const ExprAST& getLhs() const noexcept { return *m_lhs; }
    [[nodiscard]] const ExprAST& getRhs() const noexcept { return *m_rhs; }
    [[nodiscard]] ExprAST& getLhs() noexcept { return *m_lhs; }
    [[nodiscard]] ExprAST& getRhs() noexcept { return *m_rhs; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    BinaryOp m_op;
    std::unique_ptr<ExprAST> m_lhs;
    std::unique_ptr<ExprAST> m_rhs;
};

class UnaryExpr : public ExprAST {
public:
    UnaryExpr(UnaryOp op, std::unique_ptr<ExprAST> operand, SourceSpan span)
        : ExprAST(span), m_op(op), m_operand(std::move(operand)) {}

    [[nodiscard]] UnaryOp getOp() const noexcept { return m_op; }
    [[nodiscard]] const ExprAST& getOperand() const noexcept { return *m_operand; }
    [[nodiscard]] ExprAST& getOperand() noexcept { return *m_operand; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    UnaryOp m_op;
    std::unique_ptr<ExprAST> m_operand;
};

class CallExpr : public ExprAST {
public:
    CallExpr(std::string callee, std::vector<std::string> typeArgs,
             std::vector<std::unique_ptr<ExprAST>> args, SourceSpan span)
        : ExprAST(span), m_callee(std::move(callee)), m_typeArgs(std::move(typeArgs)),
          m_args(std::move(args)) {}

    CallExpr(std::string callee, std::vector<std::unique_ptr<ExprAST>> args, SourceSpan span)
        : ExprAST(span), m_callee(std::move(callee)), m_args(std::move(args)) {}

    [[nodiscard]] const std::string& getCallee() const noexcept { return m_callee; }
    void setCallee(std::string callee) noexcept { m_callee = std::move(callee); }
    [[nodiscard]] const std::vector<std::string>& getTypeArgs() const noexcept { return m_typeArgs; }
    [[nodiscard]] bool hasTypeArgs() const noexcept { return !m_typeArgs.empty(); }
    [[nodiscard]] const std::vector<std::unique_ptr<ExprAST>>& getArgs() const noexcept { return m_args; }
    [[nodiscard]] std::vector<std::unique_ptr<ExprAST>>& getArgs() noexcept { return m_args; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_callee;
    std::vector<std::string> m_typeArgs;
    std::vector<std::unique_ptr<ExprAST>> m_args;
};

class StructInitExpr : public ExprAST {
public:
    StructInitExpr(std::string structName, std::vector<std::string> typeArgs,
                   std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> fields,
                   SourceSpan span)
        : ExprAST(span), m_structName(std::move(structName)), m_typeArgs(std::move(typeArgs)),
          m_fields(std::move(fields)) {}

    StructInitExpr(std::string structName,
                   std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> fields,
                   SourceSpan span)
        : ExprAST(span), m_structName(std::move(structName)), m_fields(std::move(fields)) {}

    [[nodiscard]] const std::string& getStructName() const noexcept { return m_structName; }
    void setStructName(std::string name) noexcept { m_structName = std::move(name); }
    [[nodiscard]] const std::vector<std::string>& getTypeArgs() const noexcept { return m_typeArgs; }
    [[nodiscard]] bool hasTypeArgs() const noexcept { return !m_typeArgs.empty(); }
    [[nodiscard]] const std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>>& getFields() const noexcept {
        return m_fields;
    }
    [[nodiscard]] std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>>& getFields() noexcept {
        return m_fields;
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_structName;
    std::vector<std::string> m_typeArgs;
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> m_fields;
};

class MemberAccessExpr : public ExprAST {
public:
    MemberAccessExpr(std::unique_ptr<ExprAST> base, std::string member, SourceSpan span)
        : ExprAST(span), m_base(std::move(base)), m_member(std::move(member)) {}

    [[nodiscard]] const ExprAST& getBase() const noexcept { return *m_base; }
    [[nodiscard]] ExprAST& getBase() noexcept { return *m_base; }
    [[nodiscard]] const std::string& getMember() const noexcept { return m_member; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_base;
    std::string m_member;
};

class ArrayLiteralExpr : public ExprAST {
public:
    struct Element {
        std::unique_ptr<ExprAST> key;   // nullptr if sequential
        std::unique_ptr<ExprAST> value;
    };

    ArrayLiteralExpr(std::vector<Element> elements, SourceSpan span)
        : ExprAST(span), m_elements(std::move(elements)) {}

    ArrayLiteralExpr(std::vector<std::unique_ptr<ExprAST>> elements, SourceSpan span)
        : ExprAST(span) {
        m_elements.reserve(elements.size());
        for (auto& elem : elements) {
            m_elements.push_back(Element{nullptr, std::move(elem)});
        }
    }

    [[nodiscard]] const std::vector<Element>& getElements() const noexcept { return m_elements; }
    [[nodiscard]] std::vector<Element>& getElements() noexcept { return m_elements; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::vector<Element> m_elements;
};

class IndexExpr : public ExprAST {
public:
    IndexExpr(std::unique_ptr<ExprAST> base, std::unique_ptr<ExprAST> index, SourceSpan span)
        : ExprAST(span), m_base(std::move(base)), m_index(std::move(index)) {}

    [[nodiscard]] const ExprAST& getBase() const noexcept { return *m_base; }
    [[nodiscard]] ExprAST& getBase() noexcept { return *m_base; }
    [[nodiscard]] bool hasIndex() const noexcept { return m_index != nullptr; }
    [[nodiscard]] const ExprAST& getIndex() const noexcept { return *m_index; }
    [[nodiscard]] ExprAST& getIndex() noexcept { return *m_index; }
    [[nodiscard]] const ExprAST* getIndexPtr() const noexcept { return m_index.get(); }
    [[nodiscard]] ExprAST* getIndexPtr() noexcept { return m_index.get(); }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_base;
    std::unique_ptr<ExprAST> m_index;
};

class RangeExpr : public ExprAST {
public:
    RangeExpr(std::unique_ptr<ExprAST> start, std::unique_ptr<ExprAST> end, SourceSpan span)
        : ExprAST(span), m_start(std::move(start)), m_end(std::move(end)) {}

    [[nodiscard]] bool hasStart() const noexcept { return m_start != nullptr; }
    [[nodiscard]] bool hasEnd() const noexcept { return m_end != nullptr; }

    [[nodiscard]] const ExprAST* getStart() const noexcept { return m_start.get(); }
    [[nodiscard]] ExprAST* getStart() noexcept { return m_start.get(); }
    [[nodiscard]] const ExprAST* getEnd() const noexcept { return m_end.get(); }
    [[nodiscard]] ExprAST* getEnd() noexcept { return m_end.get(); }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_start;
    std::unique_ptr<ExprAST> m_end;
};

class SliceExpr : public ExprAST {
public:
    SliceExpr(std::unique_ptr<ExprAST> base, std::unique_ptr<RangeExpr> range, SourceSpan span)
        : ExprAST(span), m_base(std::move(base)), m_range(std::move(range)) {}

    [[nodiscard]] const ExprAST& getBase() const noexcept { return *m_base; }
    [[nodiscard]] ExprAST& getBase() noexcept { return *m_base; }
    [[nodiscard]] const RangeExpr& getRange() const noexcept { return *m_range; }
    [[nodiscard]] RangeExpr& getRange() noexcept { return *m_range; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_base;
    std::unique_ptr<RangeExpr> m_range;
};

class ScopedAccessExpr : public ExprAST {
public:
    ScopedAccessExpr(std::string scopeName, std::string memberName, SourceSpan span)
        : ExprAST(span), m_scopeName(std::move(scopeName)), m_memberName(std::move(memberName)) {}

    [[nodiscard]] const std::string& getScopeName() const noexcept { return m_scopeName; }
    [[nodiscard]] const std::string& getMemberName() const noexcept { return m_memberName; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_scopeName;
    std::string m_memberName;
};

class ThisExpr : public ExprAST {
public:
    explicit ThisExpr(SourceSpan span) : ExprAST(span) {}

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class MethodCallExpr : public ExprAST {
public:
    MethodCallExpr(std::unique_ptr<ExprAST> receiver, std::string methodName,
                   std::vector<std::unique_ptr<ExprAST>> args, SourceSpan span)
        : ExprAST(span), m_receiver(std::move(receiver)), m_methodName(std::move(methodName)),
          m_args(std::move(args)) {}

    [[nodiscard]] const ExprAST& getReceiver() const noexcept { return *m_receiver; }
    [[nodiscard]] ExprAST& getReceiver() noexcept { return *m_receiver; }
    [[nodiscard]] const std::string& getMethodName() const noexcept { return m_methodName; }
    [[nodiscard]] const std::vector<std::unique_ptr<ExprAST>>& getArgs() const noexcept { return m_args; }
    [[nodiscard]] std::vector<std::unique_ptr<ExprAST>>& getArgs() noexcept { return m_args; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_receiver;
    std::string m_methodName;
    std::vector<std::unique_ptr<ExprAST>> m_args;
};

class AssignExpr : public ExprAST {
public:
    AssignExpr(std::unique_ptr<ExprAST> target, std::unique_ptr<ExprAST> value, SourceSpan span)
        : ExprAST(span), m_target(std::move(target)), m_value(std::move(value)) {}

    [[nodiscard]] const ExprAST& getTarget() const noexcept { return *m_target; }
    [[nodiscard]] ExprAST& getTarget() noexcept { return *m_target; }
    [[nodiscard]] const ExprAST& getValue() const noexcept { return *m_value; }
    [[nodiscard]] ExprAST& getValue() noexcept { return *m_value; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_target;
    std::unique_ptr<ExprAST> m_value;
};

class CastExpr : public ExprAST {
public:
    CastExpr(std::unique_ptr<ExprAST> expr, std::string targetType, SourceSpan span)
        : ExprAST(span), m_expr(std::move(expr)), m_targetType(std::move(targetType)) {}

    [[nodiscard]] const ExprAST& getExpr() const noexcept { return *m_expr; }
    [[nodiscard]] ExprAST& getExpr() noexcept { return *m_expr; }
    [[nodiscard]] const std::string& getTargetType() const noexcept { return m_targetType; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_expr;
    std::string m_targetType;
};

class RcNewExpr : public ExprAST {
public:
    RcNewExpr(std::unique_ptr<ExprAST> value, SourceSpan span)
        : ExprAST(span), m_value(std::move(value)) {}

    [[nodiscard]] const ExprAST& getValue() const noexcept { return *m_value; }
    [[nodiscard]] ExprAST& getValue() noexcept { return *m_value; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_value;
};

class RcCountExpr : public ExprAST {
public:
    RcCountExpr(std::unique_ptr<ExprAST> pointer, SourceSpan span)
        : ExprAST(span), m_pointer(std::move(pointer)) {}

    [[nodiscard]] const ExprAST& getPointer() const noexcept { return *m_pointer; }
    [[nodiscard]] ExprAST& getPointer() noexcept { return *m_pointer; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_pointer;
};

class BoxNewExpr : public ExprAST {
public:
    BoxNewExpr(std::unique_ptr<ExprAST> value, SourceSpan span)
        : ExprAST(span), m_value(std::move(value)) {}

    [[nodiscard]] const ExprAST& getValue() const noexcept { return *m_value; }
    [[nodiscard]] ExprAST& getValue() noexcept { return *m_value; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_value;
};

class ArenaNewExpr : public ExprAST {
public:
    ArenaNewExpr(std::unique_ptr<ExprAST> capacity, SourceSpan span)
        : ExprAST(span), m_capacity(std::move(capacity)) {}

    [[nodiscard]] bool hasCapacity() const noexcept { return m_capacity != nullptr; }
    [[nodiscard]] const ExprAST& getCapacity() const noexcept { return *m_capacity; }
    [[nodiscard]] ExprAST& getCapacity() noexcept { return *m_capacity; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_capacity;
};

class ArenaAllocExpr : public ExprAST {
public:
    ArenaAllocExpr(std::unique_ptr<ExprAST> arena, std::unique_ptr<ExprAST> value, SourceSpan span)
        : ExprAST(span), m_arena(std::move(arena)), m_value(std::move(value)) {}

    [[nodiscard]] const ExprAST& getArena() const noexcept { return *m_arena; }
    [[nodiscard]] ExprAST& getArena() noexcept { return *m_arena; }
    [[nodiscard]] const ExprAST& getValue() const noexcept { return *m_value; }
    [[nodiscard]] ExprAST& getValue() noexcept { return *m_value; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_arena;
    std::unique_ptr<ExprAST> m_value;
};

class ArenaResetExpr : public ExprAST {
public:
    ArenaResetExpr(std::unique_ptr<ExprAST> arena, SourceSpan span)
        : ExprAST(span), m_arena(std::move(arena)) {}

    [[nodiscard]] const ExprAST& getArena() const noexcept { return *m_arena; }
    [[nodiscard]] ExprAST& getArena() noexcept { return *m_arena; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_arena;
};

class ArenaDestroyExpr : public ExprAST {
public:
    ArenaDestroyExpr(std::unique_ptr<ExprAST> arena, SourceSpan span)
        : ExprAST(span), m_arena(std::move(arena)) {}

    [[nodiscard]] const ExprAST& getArena() const noexcept { return *m_arena; }
    [[nodiscard]] ExprAST& getArena() noexcept { return *m_arena; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_arena;
};

class VecNewExpr : public ExprAST {
public:
    VecNewExpr(std::string elemType, std::unique_ptr<ExprAST> capacity, SourceSpan span)
        : ExprAST(span), m_elemType(std::move(elemType)), m_capacity(std::move(capacity)) {}

    [[nodiscard]] const std::string& getElemType() const noexcept { return m_elemType; }
    [[nodiscard]] bool hasCapacity() const noexcept { return m_capacity != nullptr; }
    [[nodiscard]] const ExprAST& getCapacity() const noexcept { return *m_capacity; }
    [[nodiscard]] ExprAST& getCapacity() noexcept { return *m_capacity; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_elemType;
    std::unique_ptr<ExprAST> m_capacity;
};

class NewExpr : public ExprAST {
public:
    NewExpr(std::string className, std::vector<std::unique_ptr<ExprAST>> args, SourceSpan span)
        : ExprAST(span), m_className(std::move(className)), m_args(std::move(args)) {}

    [[nodiscard]] const std::string& getClassName() const noexcept { return m_className; }
    [[nodiscard]] const std::vector<std::unique_ptr<ExprAST>>& getArgs() const noexcept { return m_args; }
    [[nodiscard]] std::vector<std::unique_ptr<ExprAST>>& getArgs() noexcept { return m_args; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_className;
    std::vector<std::unique_ptr<ExprAST>> m_args;
};

class AwaitExpr : public ExprAST {
public:
    AwaitExpr(std::unique_ptr<ExprAST> expr, SourceSpan span)
        : ExprAST(span), m_expr(std::move(expr)) {}

    [[nodiscard]] const ExprAST& getExpr() const noexcept { return *m_expr; }
    [[nodiscard]] ExprAST& getExpr() noexcept { return *m_expr; }
    [[nodiscard]] const ExprAST& getOperand() const noexcept { return *m_expr; }
    [[nodiscard]] ExprAST& getOperand() noexcept { return *m_expr; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<ExprAST> m_expr;
};

} // namespace sph

