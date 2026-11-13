#include "ASTPrinter.hpp"
#include "DeclAST.hpp"
#include "ExprAST.hpp"
#include "StmtAST.hpp"
#include <iostream>

namespace sph {

ASTPrinter::ASTPrinter(std::ostream& out) : m_out(out) {}

void ASTPrinter::print(ProgramAST& program) {
    program.accept(*this);
}

void ASTPrinter::indent() {
    for (int i = 0; i < m_indentLevel; ++i) {
        m_out << "  ";
    }
}

void ASTPrinter::visit(ProgramAST& node) {
    indent();
    m_out << "Program\n";
    ++m_indentLevel;
    for (auto& imp : node.getImports()) {
        imp->accept(*this);
    }
    for (auto& st : node.getStructs()) {
        st->accept(*this);
    }
    for (auto& en : node.getEnums()) {
        en->accept(*this);
    }
    for (auto& iface : node.getInterfaces()) {
        iface->accept(*this);
    }
    for (auto& cl : node.getClasses()) {
        cl->accept(*this);
    }
    for (auto& fn : node.getFunctions()) {
        fn->accept(*this);
    }
    --m_indentLevel;
}

void ASTPrinter::visit(ImportDecl& node) {
    indent();
    m_out << "Import: \"" << node.getPath() << "\"\n";
}

void ASTPrinter::visit(StructDecl& node) {
    indent();
    if (node.getAttribute().layout == StructLayout::Packed) {
        m_out << "#[repr(packed)] ";
    } else if (node.getAttribute().layout == StructLayout::C) {
        m_out << "#[repr(C)] ";
    }
    if (node.getAttribute().explicitAlignment > 0) {
        m_out << "#[align(" << node.getAttribute().explicitAlignment << ")] ";
    }
    m_out << "StructDecl: " << node.getName();
    if (!node.getTypeParams().empty()) {
        m_out << "<";
        for (size_t i = 0; i < node.getTypeParams().size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << node.getTypeParams()[i];
        }
        m_out << ">";
    }
    m_out << " {\n";
    ++m_indentLevel;
    for (const auto& field : node.getFields()) {
        indent();
        m_out << field.name << ": " << field.type << "\n";
    }
    --m_indentLevel;
    indent();
    m_out << "}\n";
}

void ASTPrinter::visit(EnumDecl& node) {
    indent();
    m_out << "EnumDecl: " << node.getName() << " {\n";
    ++m_indentLevel;
    for (const auto& variant : node.getVariants()) {
        indent();
        m_out << variant.name;
        if (variant.explicitValue.has_value()) {
            m_out << " = " << *variant.explicitValue;
        }
        m_out << "\n";
    }
    --m_indentLevel;
    indent();
    m_out << "}\n";
}

void ASTPrinter::visit(InterfaceDecl& node) {
    indent();
    m_out << "InterfaceDecl: " << node.getName() << " {\n";
    ++m_indentLevel;
    for (const auto& method : node.getMethods()) {
        indent();
        m_out << "fn " << method.name << "(";
        for (size_t i = 0; i < method.params.size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << method.params[i].name << ": " << method.params[i].type;
        }
        m_out << "): " << method.returnType << "\n";
    }
    --m_indentLevel;
    indent();
    m_out << "}\n";
}

void ASTPrinter::visit(ClassDecl& node) {
    indent();
    m_out << "ClassDecl: " << node.getName();
    if (!node.getImplements().empty()) {
        m_out << " implements ";
        for (size_t i = 0; i < node.getImplements().size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << node.getImplements()[i];
        }
    }
    m_out << " {\n";
    ++m_indentLevel;
    for (const auto& field : node.getFields()) {
        indent();
        m_out << field.name << ": " << field.type << "\n";
    }
    for (auto& method : node.getMethods()) {
        method->accept(*this);
    }
    --m_indentLevel;
    indent();
    m_out << "}\n";
}

void ASTPrinter::visit(FunctionDecl& node) {
    indent();
    if (node.isExtern()) {
        m_out << "ExternFunctionDecl: " << node.getName() << " [ABI: \"" << node.getAbi() << "\"](";
        const auto& params = node.getParams();
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << params[i].name << ": " << params[i].type;
        }
        m_out << ") -> " << node.getReturnType() << "\n";
        return;
    }
    if (node.isAsync()) {
        m_out << "Async ";
    }
    m_out << "FunctionDecl: " << node.getName();
    if (!node.getTypeParams().empty()) {
        m_out << "<";
        for (size_t i = 0; i < node.getTypeParams().size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << node.getTypeParams()[i];
        }
        m_out << ">";
    }
    m_out << "(";
    const auto& params = node.getParams();
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) m_out << ", ";
        m_out << params[i].name << ": " << params[i].type;
    }
    m_out << ") -> " << node.getReturnType() << "\n";

    ++m_indentLevel;
    node.getBody().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(BlockStmt& node) {
    indent();
    m_out << "Block\n";
    ++m_indentLevel;
    for (auto& stmt : node.getStatements()) {
        stmt->accept(*this);
    }
    --m_indentLevel;
}

void ASTPrinter::visit(UnsafeBlockStmt& node) {
    indent();
    m_out << "UnsafeBlock\n";
    ++m_indentLevel;
    for (auto& stmt : node.getBlock().getStatements()) {
        stmt->accept(*this);
    }
    --m_indentLevel;
}

void ASTPrinter::visit(VarDeclStmt& node) {
    indent();
    m_out << "VarDecl: " << (node.isMut() ? "mut " : "let ") << node.getName();
    if (node.getTypeAnnotation().has_value()) {
        m_out << ": " << *node.getTypeAnnotation();
    }
    m_out << " =\n";
    ++m_indentLevel;
    node.getInitializer().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(ReturnStmt& node) {
    indent();
    if (node.hasValue()) {
        m_out << "Return\n";
        ++m_indentLevel;
        node.getValue()->accept(*this);
        --m_indentLevel;
    } else {
        m_out << "Return (void)\n";
    }
}

void ASTPrinter::visit(IfStmt& node) {
    indent();
    m_out << "If\n";

    indent();
    m_out << "  Condition:\n";
    m_indentLevel += 2;
    node.getCondition().accept(*this);
    m_indentLevel -= 2;

    indent();
    m_out << "  Then:\n";
    m_indentLevel += 2;
    node.getThenBranch().accept(*this);
    m_indentLevel -= 2;

    if (node.hasElse()) {
        indent();
        m_out << "  Else:\n";
        m_indentLevel += 2;
        node.getElseBranch()->accept(*this);
        m_indentLevel -= 2;
    }
}

void ASTPrinter::visit(WhileStmt& node) {
    indent();
    m_out << "While\n";

    indent();
    m_out << "  Condition:\n";
    m_indentLevel += 2;
    node.getCondition().accept(*this);
    m_indentLevel -= 2;

    indent();
    m_out << "  Body:\n";
    m_indentLevel += 2;
    node.getBody().accept(*this);
    m_indentLevel -= 2;
}

void ASTPrinter::visit(ForeachStmt& node) {
    indent();
    m_out << "Foreach";
    if (node.hasKeyVar()) {
        m_out << " Key: " << node.getKeyVar();
    }
    m_out << " Value: " << node.getValVar() << "\n";

    indent();
    m_out << "  Collection:\n";
    m_indentLevel += 2;
    node.getCollection().accept(*this);
    m_indentLevel -= 2;

    indent();
    m_out << "  Body:\n";
    m_indentLevel += 2;
    node.getBody().accept(*this);
    m_indentLevel -= 2;
}

void ASTPrinter::visit(BreakStmt& node) {
    (void)node;
    indent();
    m_out << "Break\n";
}

void ASTPrinter::visit(ContinueStmt& node) {
    (void)node;
    indent();
    m_out << "Continue\n";
}

void ASTPrinter::visit(ExprStmt& node) {
    indent();
    m_out << "ExprStmt\n";
    ++m_indentLevel;
    node.getExpr().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(EchoStmt& node) {
    indent();
    m_out << "EchoStmt\n";
    ++m_indentLevel;
    for (const auto& expr : node.getExpressions()) {
        expr->accept(*this);
    }
    --m_indentLevel;
}

void ASTPrinter::visit(ThrowStmt& node) {
    indent();
    m_out << "ThrowStmt\n";
    ++m_indentLevel;
    node.getExpr().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(TryStmt& node) {
    indent();
    m_out << "TryStmt\n";
    indent();
    m_out << "  TryBlock:\n";
    m_indentLevel += 2;
    node.getTryBlock().accept(*this);
    m_indentLevel -= 2;

    for (const auto& clause : node.getCatchClauses()) {
        indent();
        m_out << "  CatchClause (" << (clause.type.empty() ? "" : (clause.type + " ")) << clause.varName << "):\n";
        m_indentLevel += 2;
        clause.body->accept(*this);
        m_indentLevel -= 2;
    }

    if (node.hasFinally()) {
        indent();
        m_out << "  FinallyBlock:\n";
        m_indentLevel += 2;
        node.getFinallyBlock()->accept(*this);
        m_indentLevel -= 2;
    }
}

void ASTPrinter::visit(DeferStmt& node) {
    indent();
    m_out << "DeferStmt:\n";
    m_indentLevel += 2;
    node.getStatement().accept(*this);
    m_indentLevel -= 2;
}

void ASTPrinter::visit(IntegerLiteralExpr& node) {
    indent();
    m_out << "IntegerLiteral: " << node.getValue() << "\n";
}

void ASTPrinter::visit(FloatLiteralExpr& node) {
    indent();
    m_out << "FloatLiteral: " << node.getValue() << "\n";
}

void ASTPrinter::visit(StringLiteralExpr& node) {
    indent();
    m_out << "StringLiteral: \"" << node.getValue() << "\"\n";
}

void ASTPrinter::visit(BoolLiteralExpr& node) {
    indent();
    m_out << "BoolLiteral: " << (node.getValue() ? "true" : "false") << "\n";
}

void ASTPrinter::visit(VariableExpr& node) {
    indent();
    m_out << "Variable: " << node.getName() << "\n";
}

void ASTPrinter::visit(BinaryExpr& node) {
    indent();
    m_out << "Binary: " << binaryOpToString(node.getOp()) << "\n";
    ++m_indentLevel;
    node.getLhs().accept(*this);
    node.getRhs().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(UnaryExpr& node) {
    indent();
    m_out << "Unary: " << unaryOpToString(node.getOp()) << "\n";
    ++m_indentLevel;
    node.getOperand().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(CallExpr& node) {
    indent();
    m_out << "Call: " << node.getCallee();
    if (node.hasTypeArgs()) {
        m_out << "<";
        for (size_t i = 0; i < node.getTypeArgs().size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << node.getTypeArgs()[i];
        }
        m_out << ">";
    }
    m_out << "\n";
    ++m_indentLevel;
    for (auto& arg : node.getArgs()) {
        arg->accept(*this);
    }
    --m_indentLevel;
}

void ASTPrinter::visit(AssignExpr& node) {
    indent();
    m_out << "Assign:\n";
    ++m_indentLevel;
    node.getTarget().accept(*this);
    node.getValue().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(StructInitExpr& node) {
    indent();
    m_out << "StructInit: " << node.getStructName();
    if (node.hasTypeArgs()) {
        m_out << "<";
        for (size_t i = 0; i < node.getTypeArgs().size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << node.getTypeArgs()[i];
        }
        m_out << ">";
    }
    m_out << " {\n";
    ++m_indentLevel;
    for (auto& f : node.getFields()) {
        indent();
        m_out << f.first << ":\n";
        ++m_indentLevel;
        f.second->accept(*this);
        --m_indentLevel;
    }
    --m_indentLevel;
    indent();
    m_out << "}\n";
}

void ASTPrinter::visit(MemberAccessExpr& node) {
    indent();
    m_out << "MemberAccess: ." << node.getMember() << "\n";
    ++m_indentLevel;
    node.getBase().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(ArrayLiteralExpr& node) {
    indent();
    m_out << "ArrayLiteral [\n";
    ++m_indentLevel;
    for (auto& elem : node.getElements()) {
        if (elem.key) {
            elem.key->accept(*this);
            indent();
            m_out << "=>\n";
        }
        elem.value->accept(*this);
    }
    --m_indentLevel;
    indent();
    m_out << "]\n";
}

void ASTPrinter::visit(IndexExpr& node) {
    indent();
    m_out << "IndexExpr\n";
    ++m_indentLevel;
    indent();
    m_out << "Base:\n";
    ++m_indentLevel;
    node.getBase().accept(*this);
    --m_indentLevel;
    indent();
    m_out << "Index:\n";
    ++m_indentLevel;
    if (node.hasIndex()) {
        node.getIndex().accept(*this);
    } else {
        indent();
        m_out << "<empty>\n";
    }
    --m_indentLevel;
    --m_indentLevel;
}

void ASTPrinter::visit(RangeExpr& node) {
    indent();
    m_out << "RangeExpr (..)\n";
    if (node.hasStart()) {
        indent();
        m_out << "  Start:\n";
        m_indentLevel += 2;
        const_cast<ExprAST*>(node.getStart())->accept(*this);
        m_indentLevel -= 2;
    }
    if (node.hasEnd()) {
        indent();
        m_out << "  End:\n";
        m_indentLevel += 2;
        const_cast<ExprAST*>(node.getEnd())->accept(*this);
        m_indentLevel -= 2;
    }
}

void ASTPrinter::visit(SliceExpr& node) {
    indent();
    m_out << "SliceExpr\n";
    ++m_indentLevel;
    indent();
    m_out << "Base:\n";
    ++m_indentLevel;
    node.getBase().accept(*this);
    --m_indentLevel;
    indent();
    m_out << "Range:\n";
    ++m_indentLevel;
    node.getRange().accept(*this);
    --m_indentLevel;
    --m_indentLevel;
}

void ASTPrinter::visit(ScopedAccessExpr& node) {
    indent();
    m_out << "ScopedAccess: " << node.getScopeName() << "::" << node.getMemberName() << "\n";
}

void ASTPrinter::visit(ThisExpr& /*node*/) {
    indent();
    m_out << "This\n";
}

void ASTPrinter::visit(MethodCallExpr& node) {
    indent();
    m_out << "MethodCall: ." << node.getMethodName() << "\n";
    ++m_indentLevel;
    indent();
    m_out << "Receiver:\n";
    ++m_indentLevel;
    node.getReceiver().accept(*this);
    --m_indentLevel;
    if (!node.getArgs().empty()) {
        indent();
        m_out << "Args:\n";
        ++m_indentLevel;
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
        }
        --m_indentLevel;
    }
    --m_indentLevel;
}

void ASTPrinter::visit(CastExpr& node) {
    indent();
    m_out << "Cast: " << node.getTargetType() << "\n";
    ++m_indentLevel;
    node.getExpr().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(RcNewExpr& node) {
    indent();
    m_out << "Rc::new\n";
    ++m_indentLevel;
    node.getValue().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(RcCountExpr& node) {
    indent();
    m_out << "Rc::count\n";
    ++m_indentLevel;
    node.getPointer().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(BoxNewExpr& node) {
    indent();
    m_out << "Box::new\n";
    ++m_indentLevel;
    node.getValue().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(ArenaNewExpr& node) {
    indent();
    m_out << "Arena::new\n";
    if (node.hasCapacity()) {
        ++m_indentLevel;
        node.getCapacity().accept(*this);
        --m_indentLevel;
    }
}

void ASTPrinter::visit(ArenaAllocExpr& node) {
    indent();
    m_out << "Arena::alloc\n";
    ++m_indentLevel;
    node.getArena().accept(*this);
    node.getValue().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(ArenaResetExpr& node) {
    indent();
    m_out << "Arena::reset\n";
    ++m_indentLevel;
    node.getArena().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(ArenaDestroyExpr& node) {
    indent();
    m_out << "Arena::destroy\n";
    ++m_indentLevel;
    node.getArena().accept(*this);
    --m_indentLevel;
}

void ASTPrinter::visit(VecNewExpr& node) {
    indent();
    m_out << "Vec::new<" << node.getElemType() << ">\n";
    if (node.hasCapacity()) {
        ++m_indentLevel;
        node.getCapacity().accept(*this);
        --m_indentLevel;
    }
}

void ASTPrinter::visit(NewExpr& node) {
    indent();
    m_out << "NewExpr: " << node.getClassName() << "\n";
    ++m_indentLevel;
    for (const auto& arg : node.getArgs()) {
        arg->accept(*this);
    }
    --m_indentLevel;
}

void ASTPrinter::visit(AwaitExpr& node) {
    indent();
    m_out << "AwaitExpr:\n";
    ++m_indentLevel;
    node.getOperand().accept(*this);
    --m_indentLevel;
}

} // namespace sph

