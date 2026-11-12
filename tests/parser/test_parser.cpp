#include "common/test_framework.hpp"
#include "compiler/ast/ASTPrinter.hpp"
#include "compiler/ast/DeclAST.hpp"
#include "compiler/ast/ExprAST.hpp"
#include "compiler/ast/StmtAST.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/source/SourceManager.hpp"
#include <sstream>

TEST_CASE("Parser parses SPH 0.1 acceptance program") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "\n"
        "fn add(a: i32, b: i32): i32 {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "fn main(): i32 {\n"
        "    let first: i32 = 100;\n"
        "    let second: i32 = 200;\n"
        "    let result = add(first, second);\n"
        "\n"
        "    println(result);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("hello.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();

    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);
    CHECK_EQ(program->getFunctions().size(), 2u);

    // Verify 'add' function
    const auto& fnAdd = program->getFunctions()[0];
    CHECK_EQ(fnAdd->getName(), "add");
    CHECK_EQ(fnAdd->getReturnType(), "i32");
    CHECK_EQ(fnAdd->getParams().size(), 2u);
    CHECK_EQ(fnAdd->getParams()[0].name, "a");
    CHECK_EQ(fnAdd->getParams()[0].type, "i32");
    CHECK_EQ(fnAdd->getParams()[1].name, "b");
    CHECK_EQ(fnAdd->getParams()[1].type, "i32");
    CHECK_EQ(fnAdd->getBody().getStatements().size(), 1u);

    auto* retStmt = dynamic_cast<sph::ReturnStmt*>(fnAdd->getBody().getStatements()[0].get());
    CHECK(retStmt != nullptr);
    CHECK(retStmt->hasValue());
    auto* addBin = dynamic_cast<const sph::BinaryExpr*>(retStmt->getValue());
    CHECK(addBin != nullptr);
    CHECK_EQ(static_cast<int>(addBin->getOp()), static_cast<int>(sph::BinaryOp::Add));

    // Verify 'main' function
    const auto& fnMain = program->getFunctions()[1];
    CHECK_EQ(fnMain->getName(), "main");
    CHECK_EQ(fnMain->getReturnType(), "i32");
    CHECK_EQ(fnMain->getParams().size(), 0u);
    CHECK_EQ(fnMain->getBody().getStatements().size(), 5u);

    // Verify ASTPrinter output
    std::ostringstream ss;
    sph::ASTPrinter printer(ss);
    printer.print(*program);
    std::string tree = ss.str();
    CHECK(tree.find("FunctionDecl: add(a: i32, b: i32) -> i32") != std::string::npos);
    CHECK(tree.find("FunctionDecl: main() -> i32") != std::string::npos);
    CHECK(tree.find("VarDecl: let first: i32 =") != std::string::npos);
    CHECK(tree.find("Call: add") != std::string::npos);
    CHECK(tree.find("Call: println") != std::string::npos);
}

TEST_CASE("Parser obeys operator precedence: a + b * c") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): i32 {\n"
        "    return a + b * c;\n"
        "}\n";

    uint32_t fileId = sm.addSource("prec.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    auto* ret = dynamic_cast<sph::ReturnStmt*>(program->getFunctions()[0]->getBody().getStatements()[0].get());
    CHECK(ret != nullptr);

    // Should be: (a + (b * c))
    auto* topBin = dynamic_cast<const sph::BinaryExpr*>(ret->getValue());
    CHECK(topBin != nullptr);
    CHECK_EQ(static_cast<int>(topBin->getOp()), static_cast<int>(sph::BinaryOp::Add));

    auto* lhsVar = dynamic_cast<const sph::VariableExpr*>(&topBin->getLhs());
    CHECK(lhsVar != nullptr);
    CHECK_EQ(lhsVar->getName(), "a");

    auto* rhsBin = dynamic_cast<const sph::BinaryExpr*>(&topBin->getRhs());
    CHECK(rhsBin != nullptr);
    CHECK_EQ(static_cast<int>(rhsBin->getOp()), static_cast<int>(sph::BinaryOp::Mul));
}

TEST_CASE("Parser respects parentheses in expressions: (a + b) * c") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): i32 {\n"
        "    return (a + b) * c;\n"
        "}\n";

    uint32_t fileId = sm.addSource("paren.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    auto* ret = dynamic_cast<sph::ReturnStmt*>(program->getFunctions()[0]->getBody().getStatements()[0].get());
    CHECK(ret != nullptr);

    // Should be: ((a + b) * c)
    auto* topBin = dynamic_cast<const sph::BinaryExpr*>(ret->getValue());
    CHECK(topBin != nullptr);
    CHECK_EQ(static_cast<int>(topBin->getOp()), static_cast<int>(sph::BinaryOp::Mul));

    auto* lhsBin = dynamic_cast<const sph::BinaryExpr*>(&topBin->getLhs());
    CHECK(lhsBin != nullptr);
    CHECK_EQ(static_cast<int>(lhsBin->getOp()), static_cast<int>(sph::BinaryOp::Add));
}

TEST_CASE("Parser parses if-else and while statements") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): void {\n"
        "    mut i = 0;\n"
        "    while (i < 10) {\n"
        "        if (i == 5) {\n"
        "            return;\n"
        "        } else {\n"
        "            i = i + 1;\n"
        "        }\n"
        "    }\n"
        "}\n";

    uint32_t fileId = sm.addSource("control.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    const auto& stmts = program->getFunctions()[0]->getBody().getStatements();
    CHECK_EQ(stmts.size(), 2u);

    auto* varDecl = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    CHECK(varDecl != nullptr);
    CHECK(varDecl->isMut());
    CHECK_EQ(varDecl->getName(), "i");

    auto* whileStmt = dynamic_cast<sph::WhileStmt*>(stmts[1].get());
    CHECK(whileStmt != nullptr);
    CHECK_EQ(whileStmt->getBody().getStatements().size(), 1u);

    auto* ifStmt = dynamic_cast<sph::IfStmt*>(whileStmt->getBody().getStatements()[0].get());
    CHECK(ifStmt != nullptr);
    CHECK(ifStmt->hasElse());
}

TEST_CASE("Parser reports syntax error on missing semicolon") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): i32 {\n"
        "    let x = 10\n"
        "    return x;\n"
        "}\n";

    uint32_t fileId = sm.addSource("bad_semi.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    parser.parseProgram();
    CHECK(emitter.hasErrors());
    CHECK(emitter.getErrorCount() >= 1u);
}

TEST_CASE("Parser parses struct declaration, struct initialization and member access") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "fn main(): i32 {\n"
        "    mut p = Point { x: 10, y: 20 };\n"
        "    p.x = p.y + 5;\n"
        "    return p.x;\n"
        "}\n";

    uint32_t fileId = sm.addSource("struct_test.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getStructs().size(), 1u);
    const auto& st = program->getStructs()[0];
    CHECK_EQ(st->getName(), "Point");
    CHECK_EQ(st->getFields().size(), 2u);
    CHECK_EQ(st->getFields()[0].name, "x");
    CHECK_EQ(st->getFields()[0].type, "i32");
    CHECK_EQ(st->getFields()[1].name, "y");
    CHECK_EQ(st->getFields()[1].type, "i32");

    CHECK_EQ(program->getFunctions().size(), 1u);
    const auto& stmts = program->getFunctions()[0]->getBody().getStatements();
    CHECK_EQ(stmts.size(), 3u);

    // VarDecl: mut p = Point { x: 10, y: 20 }
    auto* varDecl = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    CHECK(varDecl != nullptr);
    CHECK_EQ(varDecl->getName(), "p");
    auto* initExpr = dynamic_cast<const sph::StructInitExpr*>(&varDecl->getInitializer());
    CHECK(initExpr != nullptr);
    CHECK_EQ(initExpr->getStructName(), "Point");
    CHECK_EQ(initExpr->getFields().size(), 2u);
    CHECK_EQ(initExpr->getFields()[0].first, "x");
    CHECK_EQ(initExpr->getFields()[1].first, "y");

    // ExprStmt: p.x = p.y + 5
    auto* exprStmt = dynamic_cast<sph::ExprStmt*>(stmts[1].get());
    CHECK(exprStmt != nullptr);
    auto* assign = dynamic_cast<const sph::AssignExpr*>(&exprStmt->getExpr());
    CHECK(assign != nullptr);
    auto* memTarget = dynamic_cast<const sph::MemberAccessExpr*>(&assign->getTarget());
    CHECK(memTarget != nullptr);
    CHECK_EQ(memTarget->getMember(), "x");

    // Return: return p.x
    auto* retStmt = dynamic_cast<sph::ReturnStmt*>(stmts[2].get());
    CHECK(retStmt != nullptr);
    auto* retMem = dynamic_cast<const sph::MemberAccessExpr*>(retStmt->getValue());
    CHECK(retMem != nullptr);
    CHECK_EQ(retMem->getMember(), "x");
}

TEST_CASE("Parser parses array types, array literals and indexing") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    mut arr: [i32; 3] = [10, 20, 30];\n"
        "    arr[0] = 50;\n"
        "    return arr[1];\n"
        "}\n";

    uint32_t fileId = sm.addSource("array_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getFunctions().size(), 1u);
    const auto& stmts = program->getFunctions()[0]->getBody().getStatements();
    CHECK_EQ(stmts.size(), 3u);

    // VarDecl: mut arr: [i32; 3] = [10, 20, 30];
    auto* varDecl = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    CHECK(varDecl != nullptr);
    CHECK_EQ(varDecl->getName(), "arr");
    CHECK(varDecl->getTypeAnnotation().has_value());
    CHECK_EQ(*varDecl->getTypeAnnotation(), "[i32; 3]");

    auto* arrLit = dynamic_cast<const sph::ArrayLiteralExpr*>(&varDecl->getInitializer());
    CHECK(arrLit != nullptr);
    CHECK_EQ(arrLit->getElements().size(), 3u);

    // ExprStmt: arr[0] = 50;
    auto* exprStmt = dynamic_cast<sph::ExprStmt*>(stmts[1].get());
    CHECK(exprStmt != nullptr);
    auto* assign = dynamic_cast<const sph::AssignExpr*>(&exprStmt->getExpr());
    CHECK(assign != nullptr);
    auto* idxTarget = dynamic_cast<const sph::IndexExpr*>(&assign->getTarget());
    CHECK(idxTarget != nullptr);

    // ReturnStmt: return arr[1];
    auto* retStmt = dynamic_cast<sph::ReturnStmt*>(stmts[2].get());
    CHECK(retStmt != nullptr);
    auto* retIdx = dynamic_cast<const sph::IndexExpr*>(retStmt->getValue());
    CHECK(retIdx != nullptr);
}

TEST_CASE("Parser parses enum declarations and scoped member access") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "enum Status {\n"
        "    Pending,\n"
        "    Active = 10,\n"
        "    Completed,\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let s = Status::Active;\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("enum_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getEnums().size(), 1u);
    const auto& en = program->getEnums()[0];
    CHECK_EQ(en->getName(), "Status");
    CHECK_EQ(en->getVariants().size(), 3u);
    CHECK_EQ(en->getVariants()[0].name, "Pending");
    CHECK(!en->getVariants()[0].explicitValue.has_value());
    CHECK_EQ(en->getVariants()[1].name, "Active");
    CHECK(en->getVariants()[1].explicitValue.has_value());
    CHECK_EQ(*en->getVariants()[1].explicitValue, 10);
    CHECK_EQ(en->getVariants()[2].name, "Completed");

    CHECK_EQ(program->getFunctions().size(), 1u);
    const auto& stmts = program->getFunctions()[0]->getBody().getStatements();
    auto* varDecl = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    CHECK(varDecl != nullptr);
    CHECK_EQ(varDecl->getName(), "s");

    auto* scopedExpr = dynamic_cast<const sph::ScopedAccessExpr*>(&varDecl->getInitializer());
    CHECK(scopedExpr != nullptr);
    CHECK_EQ(scopedExpr->getScopeName(), "Status");
    CHECK_EQ(scopedExpr->getMemberName(), "Active");
}

TEST_CASE("Parser parses import statements and aggregates imported declarations") {
    sph::SourceManager sm;
    std::string helperCode =
        "<?php\n"
        "fn helperAdd(a: i32, b: i32): i32 {\n"
        "    return a + b;\n"
        "}\n";
    (void)sm.addSource("helper.sph", helperCode);

    std::string mainCode =
        "<?php\n"
        "import \"helper.sph\";\n"
        "fn main(): i32 {\n"
        "    return helperAdd(10, 20);\n"
        "}\n";
    uint32_t mainFileId = sm.addSource("main.sph", mainCode);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(mainFileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getImports().size(), 1u);
    CHECK_EQ(program->getImports()[0]->getPath(), "helper.sph");

    CHECK_EQ(program->getFunctions().size(), 2u);
    CHECK_EQ(program->getFunctions()[0]->getName(), "helperAdd");
    CHECK_EQ(program->getFunctions()[1]->getName(), "main");
}

TEST_CASE("Parser prevents infinite loops from circular imports") {
    sph::SourceManager sm;
    std::string codeA =
        "<?php\n"
        "import \"b.sph\";\n"
        "fn funcA(): void {}\n";
    (void)sm.addSource("a.sph", codeA);

    std::string codeB =
        "<?php\n"
        "import \"a.sph\";\n"
        "fn funcB(): void {}\n";
    uint32_t bFileId = sm.addSource("b.sph", codeB);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(bFileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);
    CHECK_EQ(program->getFunctions().size(), 2u);
}

TEST_CASE("Parser parses slice types and range slice expressions") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn takeSlice(s: &[i32]): &[i32] {\n"
        "    let a = s[1..4];\n"
        "    let b = s[..4];\n"
        "    let c = s[1..];\n"
        "    let d = s[..];\n"
        "    return a;\n"
        "}\n";
    uint32_t fileId = sm.addSource("slice_parse.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);
    CHECK_EQ(program->getFunctions().size(), 1u);

    const auto& fn = program->getFunctions()[0];
    CHECK_EQ(fn->getName(), "takeSlice");
    CHECK_EQ(fn->getParams().size(), 1u);
    CHECK_EQ(fn->getParams()[0].type, "&[i32]");
    CHECK_EQ(fn->getReturnType(), "&[i32]");

    const auto& body = fn->getBody().getStatements();
    CHECK_EQ(body.size(), 5u);

    auto* varA = dynamic_cast<sph::VarDeclStmt*>(body[0].get());
    CHECK(varA != nullptr);
    auto* sliceA = dynamic_cast<sph::SliceExpr*>(&varA->getInitializer());
    CHECK(sliceA != nullptr);
    CHECK(sliceA->getRange().hasStart());
    CHECK(sliceA->getRange().hasEnd());

    auto* varB = dynamic_cast<sph::VarDeclStmt*>(body[1].get());
    CHECK(varB != nullptr);
    auto* sliceB = dynamic_cast<sph::SliceExpr*>(&varB->getInitializer());
    CHECK(sliceB != nullptr);
    CHECK(!sliceB->getRange().hasStart());
    CHECK(sliceB->getRange().hasEnd());

    auto* varC = dynamic_cast<sph::VarDeclStmt*>(body[2].get());
    CHECK(varC != nullptr);
    auto* sliceC = dynamic_cast<sph::SliceExpr*>(&varC->getInitializer());
    CHECK(sliceC != nullptr);
    CHECK(sliceC->getRange().hasStart());
    CHECK(!sliceC->getRange().hasEnd());

    auto* varD = dynamic_cast<sph::VarDeclStmt*>(body[3].get());
    CHECK(varD != nullptr);
    auto* sliceD = dynamic_cast<sph::SliceExpr*>(&varD->getInitializer());
    CHECK(sliceD != nullptr);
    CHECK(!sliceD->getRange().hasStart());
    CHECK(!sliceD->getRange().hasEnd());
}

TEST_CASE("Parser parses class declarations, methods, this, and method calls") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class Counter {\n"
        "    val: i32,\n"
        "    fn inc(amount: i32): void {\n"
        "        this.val = this.val + amount;\n"
        "    }\n"
        "}\n"
        "fn main(): i32 {\n"
        "    mut c = Counter { val: 10 };\n"
        "    c.inc(5);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("class_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getClasses().size(), 1u);
    const auto& cl = program->getClasses()[0];
    CHECK_EQ(cl->getName(), "Counter");
    CHECK_EQ(cl->getFields().size(), 1u);
    CHECK_EQ(cl->getFields()[0].name, "val");
    CHECK_EQ(cl->getFields()[0].type, "i32");
    CHECK_EQ(cl->getMethods().size(), 1u);
    CHECK_EQ(cl->getMethods()[0]->getName(), "inc");
    CHECK_EQ(cl->getMethods()[0]->getParams().size(), 1u);

    // Method body: this.val = this.val + amount
    const auto& methodStmts = cl->getMethods()[0]->getBody().getStatements();
    CHECK_EQ(methodStmts.size(), 1u);
    auto* exprStmt = dynamic_cast<sph::ExprStmt*>(methodStmts[0].get());
    CHECK(exprStmt != nullptr);
    auto* assign = dynamic_cast<const sph::AssignExpr*>(&exprStmt->getExpr());
    CHECK(assign != nullptr);
    auto* targetMem = dynamic_cast<const sph::MemberAccessExpr*>(&assign->getTarget());
    CHECK(targetMem != nullptr);
    CHECK(dynamic_cast<const sph::ThisExpr*>(&targetMem->getBase()) != nullptr);

    // main statements: c.inc(5)
    CHECK_EQ(program->getFunctions().size(), 1u);
    const auto& mainStmts = program->getFunctions()[0]->getBody().getStatements();
    CHECK_EQ(mainStmts.size(), 3u);
    auto* callStmt = dynamic_cast<sph::ExprStmt*>(mainStmts[1].get());
    CHECK(callStmt != nullptr);
    auto* methodCall = dynamic_cast<const sph::MethodCallExpr*>(&callStmt->getExpr());
    CHECK(methodCall != nullptr);
    CHECK_EQ(methodCall->getMethodName(), "inc");
    CHECK_EQ(methodCall->getArgs().size(), 1u);
}

TEST_CASE("Parser parses interface declarations and class implements") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "interface Shape {\n"
        "    fn area(): i32;\n"
        "    fn scale(factor: i32): void;\n"
        "}\n"
        "class Rect implements Shape {\n"
        "    w: i32,\n"
        "    h: i32,\n"
        "    fn area(): i32 { return this.w * this.h; }\n"
        "    fn scale(factor: i32): void { this.w = this.w * factor; }\n"
        "}\n"
        "fn main(): i32 {\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("interface_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getInterfaces().size(), 1u);
    const auto& iface = program->getInterfaces()[0];
    CHECK_EQ(iface->getName(), "Shape");
    CHECK_EQ(iface->getMethods().size(), 2u);
    CHECK_EQ(iface->getMethods()[0].name, "area");
    CHECK_EQ(iface->getMethods()[0].returnType, "i32");
    CHECK_EQ(iface->getMethods()[0].params.size(), 0u);
    CHECK_EQ(iface->getMethods()[1].name, "scale");
    CHECK_EQ(iface->getMethods()[1].returnType, "void");
    CHECK_EQ(iface->getMethods()[1].params.size(), 1u);

    CHECK_EQ(program->getClasses().size(), 1u);
    const auto& cl = program->getClasses()[0];
    CHECK_EQ(cl->getName(), "Rect");
    CHECK_EQ(cl->getImplements().size(), 1u);
    CHECK_EQ(cl->getImplements()[0], "Shape");
    CHECK_EQ(cl->getMethods().size(), 2u);
}

TEST_CASE("Parser parses generic structs, generic functions, generic calls, and generic struct instantiations") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Pair<T, U> {\n"
        "    first: T,\n"
        "    second: U\n"
        "}\n"
        "fn add<T>(a: T, b: T): T {\n"
        "    return a + b;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let sum = add<i32>(10, 20);\n"
        "    let p = Pair<i32, i32> { first: 10, second: 20 };\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("generics_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    // Verify generic struct
    CHECK_EQ(program->getStructs().size(), 1u);
    const auto& st = program->getStructs()[0];
    CHECK_EQ(st->getName(), "Pair");
    CHECK(st->isGeneric());
    CHECK_EQ(st->getTypeParams().size(), 2u);
    CHECK_EQ(st->getTypeParams()[0], "T");
    CHECK_EQ(st->getTypeParams()[1], "U");
    CHECK_EQ(st->getFields().size(), 2u);
    CHECK_EQ(st->getFields()[0].name, "first");
    CHECK_EQ(st->getFields()[0].type, "T");
    CHECK_EQ(st->getFields()[1].name, "second");
    CHECK_EQ(st->getFields()[1].type, "U");

    // Verify generic function
    CHECK_EQ(program->getFunctions().size(), 2u);
    const auto& fnAdd = program->getFunctions()[0];
    CHECK_EQ(fnAdd->getName(), "add");
    CHECK(fnAdd->isGeneric());
    CHECK_EQ(fnAdd->getTypeParams().size(), 1u);
    CHECK_EQ(fnAdd->getTypeParams()[0], "T");
    CHECK_EQ(fnAdd->getParams().size(), 2u);
    CHECK_EQ(fnAdd->getParams()[0].type, "T");
    CHECK_EQ(fnAdd->getReturnType(), "T");

    // Verify generic call in main
    const auto& fnMain = program->getFunctions()[1];
    CHECK_EQ(fnMain->getBody().getStatements().size(), 3u);
    auto* varDecl1 = dynamic_cast<sph::VarDeclStmt*>(fnMain->getBody().getStatements()[0].get());
    CHECK(varDecl1 != nullptr);
    auto* callExpr = dynamic_cast<sph::CallExpr*>(&varDecl1->getInitializer());
    CHECK(callExpr != nullptr);
    CHECK_EQ(callExpr->getCallee(), "add");
    CHECK(callExpr->hasTypeArgs());
    CHECK_EQ(callExpr->getTypeArgs().size(), 1u);
    CHECK_EQ(callExpr->getTypeArgs()[0], "i32");
    CHECK_EQ(callExpr->getArgs().size(), 2u);

    // Verify generic struct instantiation in main
    auto* varDecl2 = dynamic_cast<sph::VarDeclStmt*>(fnMain->getBody().getStatements()[1].get());
    CHECK(varDecl2 != nullptr);
    auto* structInit = dynamic_cast<sph::StructInitExpr*>(&varDecl2->getInitializer());
    CHECK(structInit != nullptr);
    CHECK_EQ(structInit->getStructName(), "Pair");
    CHECK(structInit->hasTypeArgs());
    CHECK_EQ(structInit->getTypeArgs().size(), 2u);
    CHECK_EQ(structInit->getTypeArgs()[0], "i32");
    CHECK_EQ(structInit->getTypeArgs()[1], "i32");
    CHECK_EQ(structInit->getFields().size(), 2u);
}

TEST_CASE("Parser parses pointer types, address-of, deref, deref assign, and unsafe blocks") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn swap(a: *i32, b: *i32): void {\n"
        "    unsafe {\n"
        "        let temp: i32 = *a;\n"
        "        *a = *b;\n"
        "        *b = temp;\n"
        "    }\n"
        "}\n"
        "fn main(): i32 {\n"
        "    mut x: i32 = 42;\n"
        "    mut y: i32 = 99;\n"
        "    swap(&x, &y);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("pointers_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getFunctions().size(), 2u);

    // swap function
    const auto& fnSwap = program->getFunctions()[0];
    CHECK_EQ(fnSwap->getName(), "swap");
    CHECK_EQ(fnSwap->getParams().size(), 2u);
    CHECK_EQ(fnSwap->getParams()[0].name, "a");
    CHECK_EQ(fnSwap->getParams()[0].type, "*i32");
    CHECK_EQ(fnSwap->getParams()[1].name, "b");
    CHECK_EQ(fnSwap->getParams()[1].type, "*i32");
    CHECK_EQ(fnSwap->getReturnType(), "void");

    // swap body has 1 UnsafeBlockStmt
    CHECK_EQ(fnSwap->getBody().getStatements().size(), 1u);
    auto* unsafeStmt = dynamic_cast<sph::UnsafeBlockStmt*>(fnSwap->getBody().getStatements()[0].get());
    CHECK(unsafeStmt != nullptr);
    CHECK_EQ(unsafeStmt->getBlock().getStatements().size(), 3u);

    // 1. let temp: i32 = *a;
    auto* varTemp = dynamic_cast<sph::VarDeclStmt*>(unsafeStmt->getBlock().getStatements()[0].get());
    CHECK(varTemp != nullptr);
    CHECK_EQ(varTemp->getName(), "temp");
    CHECK(varTemp->getTypeAnnotation().has_value());
    CHECK_EQ(*varTemp->getTypeAnnotation(), "i32");
    auto* derefA = dynamic_cast<sph::UnaryExpr*>(&varTemp->getInitializer());
    CHECK(derefA != nullptr);
    CHECK_EQ(static_cast<int>(derefA->getOp()), static_cast<int>(sph::UnaryOp::Deref));

    // 2. *a = *b; (AssignExpr inside ExprStmt)
    auto* exprStmt1 = dynamic_cast<sph::ExprStmt*>(unsafeStmt->getBlock().getStatements()[1].get());
    CHECK(exprStmt1 != nullptr);
    auto* assign1 = dynamic_cast<sph::AssignExpr*>(&exprStmt1->getExpr());
    CHECK(assign1 != nullptr);
    auto* targetDeref1 = dynamic_cast<sph::UnaryExpr*>(&assign1->getTarget());
    CHECK(targetDeref1 != nullptr);
    CHECK_EQ(static_cast<int>(targetDeref1->getOp()), static_cast<int>(sph::UnaryOp::Deref));

    // 3. *b = temp;
    auto* exprStmt2 = dynamic_cast<sph::ExprStmt*>(unsafeStmt->getBlock().getStatements()[2].get());
    CHECK(exprStmt2 != nullptr);
    auto* assign2 = dynamic_cast<sph::AssignExpr*>(&exprStmt2->getExpr());
    CHECK(assign2 != nullptr);
    auto* targetDeref2 = dynamic_cast<sph::UnaryExpr*>(&assign2->getTarget());
    CHECK(targetDeref2 != nullptr);
    CHECK_EQ(static_cast<int>(targetDeref2->getOp()), static_cast<int>(sph::UnaryOp::Deref));

    // main function
    const auto& fnMain = program->getFunctions()[1];
    CHECK_EQ(fnMain->getBody().getStatements().size(), 4u);

    // swap(&x, &y)
    auto* callStmt = dynamic_cast<sph::ExprStmt*>(fnMain->getBody().getStatements()[2].get());
    CHECK(callStmt != nullptr);
    auto* callExpr = dynamic_cast<sph::CallExpr*>(&callStmt->getExpr());
    CHECK(callExpr != nullptr);
    CHECK_EQ(callExpr->getCallee(), "swap");
    CHECK_EQ(callExpr->getArgs().size(), 2u);

    auto* addrX = dynamic_cast<sph::UnaryExpr*>(callExpr->getArgs()[0].get());
    CHECK(addrX != nullptr);
    CHECK_EQ(static_cast<int>(addrX->getOp()), static_cast<int>(sph::UnaryOp::AddressOf));

    auto* addrY = dynamic_cast<sph::UnaryExpr*>(callExpr->getArgs()[1].get());
    CHECK(addrY != nullptr);
    CHECK_EQ(static_cast<int>(addrY->getOp()), static_cast<int>(sph::UnaryOp::AddressOf));
}

TEST_CASE("Parser parses extern C function declarations") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "extern \"C\" fn abs(n: i32): i32;\n"
        "extern fn puts(s: string): i32;\n";

    uint32_t fileId = sm.addSource("extern_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getFunctions().size(), 2u);

    // 1. abs
    const auto& fnAbs = program->getFunctions()[0];
    CHECK_EQ(fnAbs->getName(), "abs");
    CHECK(fnAbs->isExtern());
    CHECK_EQ(fnAbs->getAbi(), "C");
    CHECK(!fnAbs->hasBody());
    CHECK_EQ(fnAbs->getParams().size(), 1u);
    CHECK_EQ(fnAbs->getParams()[0].name, "n");
    CHECK_EQ(fnAbs->getParams()[0].type, "i32");
    CHECK_EQ(fnAbs->getReturnType(), "i32");

    // 2. puts (defaults to "C" ABI)
    const auto& fnPuts = program->getFunctions()[1];
    CHECK_EQ(fnPuts->getName(), "puts");
    CHECK(fnPuts->isExtern());
    CHECK_EQ(fnPuts->getAbi(), "C");
    CHECK(!fnPuts->hasBody());
    CHECK_EQ(fnPuts->getParams().size(), 1u);
    CHECK_EQ(fnPuts->getParams()[0].name, "s");
    CHECK_EQ(fnPuts->getParams()[0].type, "string");
    CHECK_EQ(fnPuts->getReturnType(), "i32");
}

TEST_CASE("Parser parses bitwise ops, shift ops, precedence, and cast expressions") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let x = a & b | c ^ ~d << 2 >> 1;\n"
        "    let y = x as i64;\n"
        "    let z = *p as i32;\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("bitwise_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);
    CHECK_EQ(program->getFunctions().size(), 1u);

    const auto& stmts = program->getFunctions()[0]->getBody().getStatements();
    auto* varX = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    CHECK(varX != nullptr);
    auto* orExpr = dynamic_cast<const sph::BinaryExpr*>(&varX->getInitializer());
    CHECK(orExpr != nullptr);
    CHECK_EQ(static_cast<int>(orExpr->getOp()), static_cast<int>(sph::BinaryOp::BitwiseOr));

    auto* varY = dynamic_cast<sph::VarDeclStmt*>(stmts[1].get());
    CHECK(varY != nullptr);
    auto* castY = dynamic_cast<const sph::CastExpr*>(&varY->getInitializer());
    CHECK(castY != nullptr);
    CHECK_EQ(castY->getTargetType(), "i64");

    auto* varZ = dynamic_cast<sph::VarDeclStmt*>(stmts[2].get());
    CHECK(varZ != nullptr);
    auto* castZ = dynamic_cast<const sph::CastExpr*>(&varZ->getInitializer());
    CHECK(castZ != nullptr);
    CHECK_EQ(castZ->getTargetType(), "i32");
    auto* deref = dynamic_cast<const sph::UnaryExpr*>(&castZ->getExpr());
    CHECK(deref != nullptr);
    CHECK_EQ(static_cast<int>(deref->getOp()), static_cast<int>(sph::UnaryOp::Deref));
}

TEST_CASE("Parser parses struct attributes: repr(packed), repr(C), and align(N)") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "#[repr(packed)]\n"
        "struct PackedHeader {\n"
        "    magic: i32,\n"
        "    ver: bool,\n"
        "}\n"
        "#[repr(C)] #[align(16)]\n"
        "struct AlignedHeader {\n"
        "    id: i64,\n"
        "}\n";

    uint32_t fileId = sm.addSource("struct_attr_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);
    CHECK_EQ(program->getStructs().size(), 2u);

    const auto& s1 = program->getStructs()[0];
    CHECK_EQ(s1->getName(), "PackedHeader");
    CHECK(s1->isPacked());
    CHECK_EQ(static_cast<int>(s1->getAttribute().layout), static_cast<int>(sph::StructLayout::Packed));

    const auto& s2 = program->getStructs()[1];
    CHECK_EQ(s2->getName(), "AlignedHeader");
    CHECK(!s2->isPacked());
    CHECK_EQ(static_cast<int>(s2->getAttribute().layout), static_cast<int>(sph::StructLayout::C));
    CHECK_EQ(s2->getAttribute().explicitAlignment, 16u);
}

TEST_CASE("Parser parses Rc types, Rc::new, and Rc::count") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Point { x: i32, y: i32 }\n"
        "fn main(): i32 {\n"
        "    let p: Rc<Point> = Rc::new(Point { x: 10, y: 20 });\n"
        "    let c: i64 = Rc::count(p);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("rc_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getFunctions().size(), 1u);
    const auto& fn = program->getFunctions()[0];
    const auto& stmts = fn->getBody().getStatements();
    CHECK_EQ(stmts.size(), 3u);

    auto* varP = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    CHECK(varP != nullptr);
    CHECK(varP->getTypeAnnotation().has_value());
    CHECK_EQ(*varP->getTypeAnnotation(), "Rc<Point>");
    auto* rcNew = dynamic_cast<const sph::RcNewExpr*>(&varP->getInitializer());
    CHECK(rcNew != nullptr);

    auto* varC = dynamic_cast<sph::VarDeclStmt*>(stmts[1].get());
    CHECK(varC != nullptr);
    CHECK_EQ(*varC->getTypeAnnotation(), "i64");
    auto* rcCount = dynamic_cast<const sph::RcCountExpr*>(&varC->getInitializer());
    CHECK(rcCount != nullptr);
}

TEST_CASE("Parser parses Box types and Box::new") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Node { value: i32 }\n"
        "fn main(): i32 {\n"
        "    let b: Box<Node> = Box::new(Node { value: 42 });\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("box_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getFunctions().size(), 1u);
    const auto& fn = program->getFunctions()[0];
    const auto& stmts = fn->getBody().getStatements();
    CHECK_EQ(stmts.size(), 2u);

    auto* varB = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    CHECK(varB != nullptr);
    CHECK(varB->getTypeAnnotation().has_value());
    CHECK_EQ(*varB->getTypeAnnotation(), "Box<Node>");
    auto* boxNew = dynamic_cast<const sph::BoxNewExpr*>(&varB->getInitializer());
    CHECK(boxNew != nullptr);
}

TEST_CASE("Parser parses Arena types and Arena functions") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Node { value: i32 }\n"
        "fn main(): i32 {\n"
        "    let mut a: Arena = Arena::new(1024);\n"
        "    let p: *Node = Arena::alloc(a, Node { value: 42 });\n"
        "    Arena::reset(a);\n"
        "    Arena::destroy(a);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("arena_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getFunctions().size(), 1u);
    const auto& fn = program->getFunctions()[0];
    const auto& stmts = fn->getBody().getStatements();
    CHECK_EQ(stmts.size(), 5u);

    auto* varA = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    CHECK(varA != nullptr);
    CHECK(varA->getTypeAnnotation().has_value());
    CHECK_EQ(*varA->getTypeAnnotation(), "Arena");
    auto* arenaNew = dynamic_cast<const sph::ArenaNewExpr*>(&varA->getInitializer());
    CHECK(arenaNew != nullptr);
    CHECK(arenaNew->hasCapacity());

    auto* varP = dynamic_cast<sph::VarDeclStmt*>(stmts[1].get());
    CHECK(varP != nullptr);
    CHECK(varP->getTypeAnnotation().has_value());
    CHECK_EQ(*varP->getTypeAnnotation(), "*Node");
    auto* arenaAlloc = dynamic_cast<const sph::ArenaAllocExpr*>(&varP->getInitializer());
    CHECK(arenaAlloc != nullptr);

    auto* resetStmt = dynamic_cast<sph::ExprStmt*>(stmts[2].get());
    CHECK(resetStmt != nullptr);
    auto* arenaReset = dynamic_cast<const sph::ArenaResetExpr*>(&resetStmt->getExpr());
    CHECK(arenaReset != nullptr);

    auto* destroyStmt = dynamic_cast<sph::ExprStmt*>(stmts[3].get());
    CHECK(destroyStmt != nullptr);
    auto* arenaDestroy = dynamic_cast<const sph::ArenaDestroyExpr*>(&destroyStmt->getExpr());
    CHECK(arenaDestroy != nullptr);
}

TEST_CASE("Parser parses Vec types, Vec::new, and Vec::with_capacity") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let mut v1: Vec<i32> = Vec::new();\n"
        "    let mut v2: Vec<f64> = Vec::with_capacity(16);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("vec_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getFunctions().size(), 1u);
    const auto& fn = program->getFunctions()[0];
    const auto& stmts = fn->getBody().getStatements();
    CHECK_EQ(stmts.size(), 3u);

    auto* varV1 = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    CHECK(varV1 != nullptr);
    CHECK(varV1->getTypeAnnotation().has_value());
    CHECK_EQ(*varV1->getTypeAnnotation(), "Vec<i32>");
    auto* vecNew1 = dynamic_cast<const sph::VecNewExpr*>(&varV1->getInitializer());
    CHECK(vecNew1 != nullptr);
    CHECK(!vecNew1->hasCapacity());

    auto* varV2 = dynamic_cast<sph::VarDeclStmt*>(stmts[1].get());
    CHECK(varV2 != nullptr);
    CHECK(varV2->getTypeAnnotation().has_value());
    CHECK_EQ(*varV2->getTypeAnnotation(), "Vec<f64>");
    auto* vecNew2 = dynamic_cast<const sph::VecNewExpr*>(&varV2->getInitializer());
    CHECK(vecNew2 != nullptr);
    CHECK(vecNew2->hasCapacity());
}

TEST_CASE("Parser parses static scoped function calls for File, Process, and Env APIs") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let data = File::read(\"test.txt\");\n"
        "    let ok = File::write(\"out.txt\", \"content\");\n"
        "    let val = Env::get(\"PATH\");\n"
        "    Process::exit(0);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("file_env_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getFunctions().size(), 1u);
    const auto& fn = program->getFunctions()[0];
    const auto& stmts = fn->getBody().getStatements();
    CHECK_EQ(stmts.size(), 5u);

    auto* s0 = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    CHECK(s0 != nullptr);
    auto* call0 = dynamic_cast<const sph::CallExpr*>(&s0->getInitializer());
    CHECK(call0 != nullptr);
    CHECK_EQ(call0->getCallee(), "File::read");
    CHECK_EQ(call0->getArgs().size(), 1u);

    auto* s1 = dynamic_cast<sph::VarDeclStmt*>(stmts[1].get());
    CHECK(s1 != nullptr);
    auto* call1 = dynamic_cast<const sph::CallExpr*>(&s1->getInitializer());
    CHECK(call1 != nullptr);
    CHECK_EQ(call1->getCallee(), "File::write");
    CHECK_EQ(call1->getArgs().size(), 2u);

    auto* s2 = dynamic_cast<sph::VarDeclStmt*>(stmts[2].get());
    CHECK(s2 != nullptr);
    auto* call2 = dynamic_cast<const sph::CallExpr*>(&s2->getInitializer());
    CHECK(call2 != nullptr);
    CHECK_EQ(call2->getCallee(), "Env::get");
    CHECK_EQ(call2->getArgs().size(), 1u);

    auto* s3 = dynamic_cast<sph::ExprStmt*>(stmts[3].get());
    CHECK(s3 != nullptr);
    auto* call3 = dynamic_cast<const sph::CallExpr*>(&s3->getExpr());
    CHECK(call3 != nullptr);
    CHECK_EQ(call3->getCallee(), "Process::exit");
    CHECK_EQ(call3->getArgs().size(), 1u);
}

TEST_CASE("Parser parses Result types, Ok, Err, and Result method calls") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn divide(a: i32, b: i32): Result<i32, string> {\n"
        "    if (b == 0) {\n"
        "        return Err(\"zero\");\n"
        "    }\n"
        "    return Ok(a / b);\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let r: Result<i32, string> = divide(10, 2);\n"
        "    let ok: bool = r.is_ok();\n"
        "    let val: i32 = r.unwrap();\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("result_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getFunctions().size(), 2u);
    const auto& fn0 = program->getFunctions()[0];
    CHECK_EQ(fn0->getName(), "divide");
    CHECK_EQ(fn0->getReturnType(), "Result<i32, string>");

    const auto& fn1 = program->getFunctions()[1];
    CHECK_EQ(fn1->getName(), "main");
    const auto& stmts = fn1->getBody().getStatements();
    CHECK_EQ(stmts.size(), 4u);
}

TEST_CASE("Parser parses echo statement and dot concatenation") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    $msg = \"Hello \" . $user;\n"
        "    echo $msg, \"\\n\";\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("echo_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    const auto& fn = program->getFunctions()[0];
    const auto& stmts = fn->getBody().getStatements();
    CHECK_EQ(stmts.size(), 3u);

    auto* assignStmt = dynamic_cast<sph::ExprStmt*>(stmts[0].get());
    CHECK(assignStmt != nullptr);
    auto* assignExpr = dynamic_cast<const sph::AssignExpr*>(&assignStmt->getExpr());
    CHECK(assignExpr != nullptr);
    auto* binExpr = dynamic_cast<const sph::BinaryExpr*>(&assignExpr->getValue());
    CHECK(binExpr != nullptr);
    CHECK(binExpr->getOp() == sph::BinaryOp::Concat);

    auto* echoStmt = dynamic_cast<sph::EchoStmt*>(stmts[1].get());
    CHECK(echoStmt != nullptr);
    CHECK_EQ(echoStmt->getExpressions().size(), 2u);
}

TEST_CASE("Parser parses associative array literals and empty bracket append") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    $arr = [\"key\" => \"value\", 42 => 100,];\n"
        "    $arr[] = 200;\n"
        "    $val = $arr[\"key\"];\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("assoc_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    const auto& fn = program->getFunctions()[0];
    const auto& stmts = fn->getBody().getStatements();
    CHECK_EQ(stmts.size(), 4u);

    // Stmt 0: $arr = ["key" => "value", 42 => 100,];
    auto* s0 = dynamic_cast<sph::ExprStmt*>(stmts[0].get());
    CHECK(s0 != nullptr);
    auto* a0 = dynamic_cast<const sph::AssignExpr*>(&s0->getExpr());
    CHECK(a0 != nullptr);
    auto* arrLit = dynamic_cast<const sph::ArrayLiteralExpr*>(&a0->getValue());
    CHECK(arrLit != nullptr);
    CHECK_EQ(arrLit->getElements().size(), 2u);
    CHECK(arrLit->getElements()[0].key != nullptr);
    CHECK(arrLit->getElements()[1].key != nullptr);

    // Stmt 1: $arr[] = 200;
    auto* s1 = dynamic_cast<sph::ExprStmt*>(stmts[1].get());
    CHECK(s1 != nullptr);
    auto* a1 = dynamic_cast<const sph::AssignExpr*>(&s1->getExpr());
    CHECK(a1 != nullptr);
    auto* idx1 = dynamic_cast<const sph::IndexExpr*>(&a1->getTarget());
    CHECK(idx1 != nullptr);
    CHECK(!idx1->hasIndex());

    // Stmt 2: $val = $arr["key"];
    auto* s2 = dynamic_cast<sph::ExprStmt*>(stmts[2].get());
    CHECK(s2 != nullptr);
    auto* a2 = dynamic_cast<const sph::AssignExpr*>(&s2->getExpr());
    CHECK(a2 != nullptr);
    auto* idx2 = dynamic_cast<const sph::IndexExpr*>(&a2->getValue());
    CHECK(idx2 != nullptr);
    CHECK(idx2->hasIndex());
}

TEST_CASE("Parser parses foreach statement with value and key-value") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    foreach ($fruits as $fruit) {\n"
        "        echo $fruit;\n"
        "    }\n"
        "    foreach ($dict as $k => $v) {\n"
        "        echo $k, $v;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("foreach_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    const auto& fn = program->getFunctions()[0];
    const auto& stmts = fn->getBody().getStatements();
    CHECK_EQ(stmts.size(), 3u);

    // Stmt 0: foreach ($fruits as $fruit)
    auto* fe0 = dynamic_cast<sph::ForeachStmt*>(stmts[0].get());
    CHECK(fe0 != nullptr);
    CHECK(!fe0->hasKeyVar());
    CHECK_EQ(fe0->getValVar(), "$fruit");

    // Stmt 1: foreach ($dict as $k => $v)
    auto* fe1 = dynamic_cast<sph::ForeachStmt*>(stmts[1].get());
    CHECK(fe1 != nullptr);
    CHECK(fe1->hasKeyVar());
    CHECK_EQ(fe1->getKeyVar(), "$k");
    CHECK_EQ(fe1->getValVar(), "$v");
}

TEST_CASE("Parser parses break and continue statements") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    while (true) {\n"
        "        if (1) {\n"
        "            break;\n"
        "        } else {\n"
        "            continue;\n"
        "        }\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("break_continue_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);
}

TEST_CASE("Parser parses new expressions, arrow operator, and PHP class syntax") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class Person {\n"
        "    public $name;\n"
        "    public $age;\n"
        "    function __construct($name, $age) {\n"
        "        $this->name = $name;\n"
        "        $this->age = $age;\n"
        "    }\n"
        "    public function greet(): string {\n"
        "        return \"Hello \" . $this->name;\n"
        "    }\n"
        "}\n"
        "fn main(): i32 {\n"
        "    $p = new Person(\"Alice\", 30);\n"
        "    let msg = $p->greet();\n"
        "    $p->age = 31;\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("php_class_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getClasses().size(), 1u);
    const auto& cls = program->getClasses()[0];
    CHECK_EQ(cls->getName(), "Person");
    CHECK_EQ(cls->getFields().size(), 2u);
    CHECK_EQ(cls->getFields()[0].name, "name");
    CHECK_EQ(cls->getFields()[1].name, "age");
    CHECK_EQ(cls->getMethods().size(), 2u);
    CHECK_EQ(cls->getMethods()[0]->getName(), "__construct");
    CHECK_EQ(cls->getMethods()[1]->getName(), "greet");

    CHECK_EQ(program->getFunctions().size(), 1u);
}

TEST_CASE("Parser parses try, catch, finally, throw, and reference parameters") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "function addOne(&$val: i32) {\n"
        "    $val = $val + 1;\n"
        "}\n"
        "function compute(i32 &$n) {\n"
        "    if ($n < 0) {\n"
        "        throw new Exception(\"negative\");\n"
        "    }\n"
        "}\n"
        "function main() {\n"
        "    try {\n"
        "        $x = 10;\n"
        "        addOne($x);\n"
        "        throw new Exception(\"boom\");\n"
        "    } catch (Exception $e) {\n"
        "        echo $e->getMessage();\n"
        "    } finally {\n"
        "        echo \"done\";\n"
        "    }\n"
        "}\n";

    uint32_t fileId = sm.addSource("exceptions_parse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    CHECK_EQ(program->getFunctions().size(), 3u);
    const auto& fn0 = program->getFunctions()[0];
    CHECK_EQ(fn0->getName(), "addOne");
    CHECK_EQ(fn0->getParams().size(), 1u);
    CHECK(fn0->getParams()[0].isByRef);
    CHECK_EQ(fn0->getParams()[0].name, "$val");

    const auto& fn1 = program->getFunctions()[1];
    CHECK_EQ(fn1->getName(), "compute");
    CHECK_EQ(fn1->getParams().size(), 1u);
    CHECK(fn1->getParams()[0].isByRef);
    CHECK_EQ(fn1->getParams()[0].name, "$n");

    const auto& fn2 = program->getFunctions()[2];
    CHECK_EQ(fn2->getName(), "main");
    CHECK_EQ(fn2->getBody().getStatements().size(), 1u);
    const auto* tryStmt = dynamic_cast<const sph::TryStmt*>(fn2->getBody().getStatements()[0].get());
    CHECK(tryStmt != nullptr);
    CHECK_EQ(tryStmt->getCatchClauses().size(), 1u);
    CHECK_EQ(tryStmt->getCatchClauses()[0].type, "Exception");
    CHECK_EQ(tryStmt->getCatchClauses()[0].varName, "$e");
    CHECK(tryStmt->hasFinally());
}




