#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/ir/IRPrinter.hpp"
#include "compiler/ir/IRVerifier.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <sstream>

TEST_CASE("IRGenerator generates verified IR for SPH 0.1 acceptance program") {
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
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool semOk = analyzer.analyze(*program);
    CHECK(semOk);

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "hello_module");
    CHECK(irModule != nullptr);
    CHECK_EQ(irModule->getFunctions().size(), 2u);

    // Verify IR invariants
    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    // Verify @add function structure
    const auto* fnAdd = irModule->getFunction("add");
    CHECK(fnAdd != nullptr);
    CHECK_EQ(fnAdd->getParameters().size(), 2u);
    CHECK(fnAdd->getEntryBlock() != nullptr);
    CHECK(fnAdd->getEntryBlock()->hasTerminator());

    // Verify @main function structure
    const auto* fnMain = irModule->getFunction("main");
    CHECK(fnMain != nullptr);
    CHECK_EQ(fnMain->getParameters().size(), 0u);
    CHECK(fnMain->getEntryBlock() != nullptr);
    CHECK(fnMain->getEntryBlock()->hasTerminator());

    // Check textual IR rendering
    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*irModule);
    std::string irText = ss.str();

    CHECK(irText.find("fn @add") != std::string::npos);
    CHECK(irText.find("fn @main") != std::string::npos);
    CHECK(irText.find("alloca i32") != std::string::npos);
    CHECK(irText.find("call i32 @add") != std::string::npos);
    CHECK(irText.find("call void @println") != std::string::npos);
    CHECK(irText.find("ret i32") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for if-else control flow") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn checkSign(val: i32): i32 {\n"
        "    if (val > 0) {\n"
        "        return 1;\n"
        "    } else {\n"
        "        return 0;\n"
        "    }\n"
        "}\n";

    uint32_t fileId = sm.addSource("ifelse.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "ifelse_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    const auto* fn = irModule->getFunction("checkSign");
    CHECK(fn != nullptr);
    // Should have entry, if.then, if.else, if.merge
    CHECK(fn->getBlocks().size() >= 3u);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*fn);
    std::string fnText = ss.str();
    CHECK(fnText.find("cond_br") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for while loop") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn countLoop(): void {\n"
        "    mut i = 0;\n"
        "    while (i < 5) {\n"
        "        i = i + 1;\n"
        "    }\n"
        "}\n";

    uint32_t fileId = sm.addSource("while.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "while_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    const auto* fn = irModule->getFunction("countLoop");
    CHECK(fn != nullptr);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*fn);
    std::string fnText = ss.str();
    CHECK(fnText.find("while.cond") != std::string::npos);
    CHECK(fnText.find("while.body") != std::string::npos);
    CHECK(fnText.find("while.exit") != std::string::npos);
}

TEST_CASE("IRVerifier detects unterminated basic block") {
    auto fn = std::make_unique<sph::IRFunction>("badFn", sph::Type::getVoid(), std::vector<std::shared_ptr<sph::IRParameter>>{});
    auto* bb = fn->createBlock("entry");
    // Add instruction without a terminator
    bb->addInstruction(std::make_unique<sph::AllocaInst>(sph::Type::getI32(), "%x"));

    sph::IRModule mod("bad_mod");
    mod.addFunction(std::move(fn));

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(mod, errors);

    CHECK(!ok);
    CHECK(!errors.empty());
    CHECK(errors[0].find("does not end with a terminator") != std::string::npos);
}

TEST_CASE("IRVerifier detects return type mismatch") {
    // Function returning i32, but ret instruction returns void
    auto fn = std::make_unique<sph::IRFunction>("badRetFn", sph::Type::getI32(), std::vector<std::shared_ptr<sph::IRParameter>>{});
    auto* bb = fn->createBlock("entry");
    bb->addInstruction(std::make_unique<sph::RetInst>()); // ret void in non-void function

    sph::IRModule mod("bad_ret_mod");
    mod.addFunction(std::move(fn));

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool ok = verifier.verify(mod, errors);

    CHECK(!ok);
    CHECK(!errors.empty());
}

TEST_CASE("IRGenerator generates verified IR for struct operations") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "fn testStruct(): i32 {\n"
        "    mut p = Point { x: 10, y: 20 };\n"
        "    p.x = p.x + 5;\n"
        "    return p.x;\n"
        "}\n";

    uint32_t fileId = sm.addSource("struct_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "struct_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    const auto* fn = irModule->getFunction("testStruct");
    CHECK(fn != nullptr);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*fn);
    std::string fnText = ss.str();

    CHECK(fnText.find("getfieldptr") != std::string::npos);
    CHECK(fnText.find("alloca Point") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for array operations") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn testArray(): i32 {\n"
        "    mut arr: [i32; 3] = [10, 20, 30];\n"
        "    arr[1] = 42;\n"
        "    return arr[1];\n"
        "}\n";

    uint32_t fileId = sm.addSource("array_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "array_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    const auto* fn = irModule->getFunction("testArray");
    CHECK(fn != nullptr);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*fn);
    std::string fnText = ss.str();

    CHECK(fnText.find("getelementptr") != std::string::npos);
    CHECK(fnText.find("alloca [i32; 3]") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for enum operations") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "enum Status {\n"
        "    Pending,\n"
        "    Active = 10,\n"
        "}\n"
        "fn checkStatus(s: Status): bool {\n"
        "    return s == Status::Active;\n"
        "}\n";

    uint32_t fileId = sm.addSource("enum_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "enum_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    const auto* fn = irModule->getFunction("checkStatus");
    CHECK(fn != nullptr);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*fn);
    std::string fnText = ss.str();

    CHECK(fnText.find("fn @checkStatus") != std::string::npos);
    CHECK(fnText.find("eq") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for slice operations") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn processSlice(s: &[i32]): i32 {\n"
        "    return s.len + s[0];\n"
        "}\n"
        "fn testSlice(): i32 {\n"
        "    let arr: [i32; 4] = [10, 20, 30, 40];\n"
        "    let sub = arr[1..3];\n"
        "    return processSlice(sub);\n"
        "}\n";

    uint32_t fileId = sm.addSource("slice_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "slice_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    const auto* fn = irModule->getFunction("testSlice");
    CHECK(fn != nullptr);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*fn);
    std::string fnText = ss.str();

    CHECK(fnText.find("alloca &[i32]") != std::string::npos);
    CHECK(fnText.find("getfieldptr") != std::string::npos);
    CHECK(fnText.find("getelementptr") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for class methods and calls") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class Counter {\n"
        "    count: i32,\n"
        "    fn inc(step: i32): void {\n"
        "        this.count = this.count + step;\n"
        "    }\n"
        "    fn get(): i32 {\n"
        "        return this.count;\n"
        "    }\n"
        "}\n"
        "fn testClass(): i32 {\n"
        "    mut c = Counter { count: 10 };\n"
        "    c.inc(5);\n"
        "    return c.get();\n"
        "}\n";

    uint32_t fileId = sm.addSource("class_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "class_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    const auto* incFn = irModule->getFunction("Counter_inc");
    CHECK(incFn != nullptr);
    const auto* getFn = irModule->getFunction("Counter_get");
    CHECK(getFn != nullptr);
    const auto* testFn = irModule->getFunction("testClass");
    CHECK(testFn != nullptr);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*testFn);
    std::string fnText = ss.str();

    CHECK(fnText.find("call void @Counter_inc") != std::string::npos);
    CHECK(fnText.find("call i32 @Counter_get") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for interfaces and dynamic dispatch") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "interface Measurable {\n"
        "    fn measure(): i32;\n"
        "}\n"
        "class Container implements Measurable {\n"
        "    size: i32,\n"
        "    fn measure(): i32 {\n"
        "        return this.size;\n"
        "    }\n"
        "}\n"
        "fn runMeasure(m: Measurable): i32 {\n"
        "    return m.measure();\n"
        "}\n"
        "fn testIface(): i32 {\n"
        "    let b = Container { size: 42 };\n"
        "    return runMeasure(b);\n"
        "}\n";

    uint32_t fileId = sm.addSource("iface_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "iface_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    // Verify vtable presence
    CHECK_EQ(irModule->getVTables().size(), 1u);
    CHECK_EQ(irModule->getVTables()[0].name, "Container_Measurable_vtable");
    CHECK_EQ(irModule->getVTables()[0].functionNames.size(), 1u);
    CHECK_EQ(irModule->getVTables()[0].functionNames[0], "Container_measure");

    const auto* runFn = irModule->getFunction("runMeasure");
    CHECK(runFn != nullptr);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*runFn);
    std::string runText = ss.str();

    // Verify indirect call through vtable
    CHECK(runText.find("getelementptr") != std::string::npos);
    CHECK(runText.find("call i32 %") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for monomorphized generic functions and structs") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Pair<T, U> {\n"
        "    first: T,\n"
        "    second: U,\n"
        "}\n"
        "fn makePair<T, U>(a: T, b: U): Pair<T, U> {\n"
        "    return Pair<T, U> { first: a, second: b };\n"
        "}\n"
        "fn add<T>(a: T, b: T): T {\n"
        "    return a + b;\n"
        "}\n"
        "fn testGenerics(): i32 {\n"
        "    let p = makePair<i32, i32>(10, 20);\n"
        "    return add<i32>(p.first, p.second);\n"
        "}\n";

    uint32_t fileId = sm.addSource("gen_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "generics_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    // Verify specialized functions exist in IR
    const auto* addFn = irModule->getFunction("add_i32");
    CHECK(addFn != nullptr);
    CHECK_EQ(addFn->getReturnType()->toString(), "i32");
    CHECK_EQ(addFn->getParameters().size(), 2u);

    const auto* makePairFn = irModule->getFunction("makePair_i32_i32");
    CHECK(makePairFn != nullptr);
    CHECK(makePairFn->getReturnType()->isStruct());
    CHECK_EQ(makePairFn->getParameters().size(), 2u);

    const auto* testFn = irModule->getFunction("testGenerics");
    CHECK(testFn != nullptr);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*testFn);
    std::string testText = ss.str();

    CHECK(testText.find("call Pair_i32_i32 @makePair_i32_i32") != std::string::npos);
    CHECK(testText.find("call i32 @add_i32") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for pointer operations and unsafe blocks") {
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

    uint32_t fileId = sm.addSource("pointers_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "pointers_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    for (const auto& err : errors) {
        std::cout << "IR Error: " << err << "\n";
    }
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    const auto* swapFn = irModule->getFunction("swap");
    CHECK(swapFn != nullptr);
    CHECK_EQ(swapFn->getParameters().size(), 2u);
    CHECK_EQ(swapFn->getParameters()[0]->getType()->toString(), "*i32");
    CHECK_EQ(swapFn->getParameters()[1]->getType()->toString(), "*i32");

    std::ostringstream ssSwap;
    sph::IRPrinter printerSwap(ssSwap);
    printerSwap.print(*swapFn);
    std::string swapText = ssSwap.str();

    CHECK(swapText.find("load *i32") != std::string::npos);
    CHECK(swapText.find("load i32") != std::string::npos);
    CHECK(swapText.find("store i32") != std::string::npos);

    const auto* mainFn = irModule->getFunction("main");
    CHECK(mainFn != nullptr);

    std::ostringstream ssMain;
    sph::IRPrinter printerMain(ssMain);
    printerMain.print(*mainFn);
    std::string mainText = ssMain.str();

    CHECK(mainText.find("addressof") != std::string::npos);
    CHECK(mainText.find("call void @swap") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR declarations for extern functions") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "extern \"C\" fn abs(n: i32): i32;\n"
        "extern fn puts(s: string): i32;\n"
        "fn testExtern(): i32 {\n"
        "    let x: i32 = -5;\n"
        "    return abs(x);\n"
        "}\n";

    uint32_t fileId = sm.addSource("extern_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "extern_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    for (const auto& err : errors) {
        std::cout << "IR Error: " << err << "\n";
    }
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    // abs declaration
    const auto* absFn = irModule->getFunction("abs");
    CHECK(absFn != nullptr);
    CHECK(absFn->isDeclaration());
    CHECK_EQ(absFn->getReturnType()->toString(), "i32");
    CHECK_EQ(absFn->getParameters().size(), 1u);

    // puts declaration
    const auto* putsFn = irModule->getFunction("puts");
    CHECK(putsFn != nullptr);
    CHECK(putsFn->isDeclaration());
    CHECK_EQ(putsFn->getReturnType()->toString(), "i32");
    CHECK_EQ(putsFn->getParameters().size(), 1u);

    // testExtern definition
    const auto* testFn = irModule->getFunction("testExtern");
    CHECK(testFn != nullptr);
    CHECK(!testFn->isDeclaration());

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*irModule);
    std::string moduleText = ss.str();

    CHECK(moduleText.find("declare @abs(%arg_n: i32): i32") != std::string::npos);
    CHECK(moduleText.find("declare @puts(%arg_s: string): i32") != std::string::npos);
    CHECK(moduleText.find("fn @testExtern(): i32") != std::string::npos);
    CHECK(moduleText.find("call i32 @abs") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for bitwise ops, pointer arithmetic, and casts") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn testBitwisePtr(): i32 {\n"
        "    let a: i32 = 12;\n"
        "    let b: i32 = 10;\n"
        "    let bit_and: i32 = a & b;\n"
        "    let bit_or: i32 = a | b;\n"
        "    let bit_xor: i32 = a ^ b;\n"
        "    let bit_shl: i32 = a << 1;\n"
        "    let bit_shr: i32 = a >> 1;\n"
        "    let bit_not: i32 = ~a;\n"
        "    let c: i64 = a as i64;\n"
        "    mut arr: [i32; 3] = [10, 20, 30];\n"
        "    unsafe {\n"
        "        let p: *i32 = &arr[0];\n"
        "        let p_next: *i32 = p + 1;\n"
        "        let p_prev: *i32 = p_next - 1;\n"
        "        let diff: i64 = p_next - p;\n"
        "        let addr: i64 = p as i64;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("bitwise_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "bitwise_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    for (const auto& err : errors) {
        std::cout << "IR Error: " << err << "\n";
    }
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*irModule);
    std::string moduleText = ss.str();

    CHECK(moduleText.find("bit_and i32") != std::string::npos);
    CHECK(moduleText.find("bit_or i32") != std::string::npos);
    CHECK(moduleText.find("bit_xor i32") != std::string::npos);
    CHECK(moduleText.find("shl i32") != std::string::npos);
    CHECK(moduleText.find("shr i32") != std::string::npos);
    CHECK(moduleText.find("bit_not i32") != std::string::npos);
    CHECK(moduleText.find("cast i32") != std::string::npos);
    CHECK(moduleText.find("getelementptr") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for Rc operations") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Point { x: i32, y: i32 }\n"
        "fn main(): i32 {\n"
        "    let p: Rc<Point> = Rc::new(Point { x: 10, y: 20 });\n"
        "    let q: Rc<Point> = p;\n"
        "    return p.x;\n"
        "}\n";

    uint32_t fileId = sm.addSource("rc_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "rc_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    for (const auto& err : errors) {
        std::cout << "IR Error: " << err << "\n";
    }
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*irModule);
    std::string moduleText = ss.str();

    CHECK(moduleText.find("rc_alloc Point (8 bytes)") != std::string::npos);
    CHECK(moduleText.find("rc_retain Rc<Point>") != std::string::npos);
    CHECK(moduleText.find("rc_release Rc<Point>") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for foreach, break, and continue") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    $arr = [1, 2, 3];\n"
        "    foreach ($arr as $x) {\n"
        "        if ($x == 2) {\n"
        "            break;\n"
        "        }\n"
        "        continue;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("foreach_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "foreach_ir_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    for (const auto& err : errors) {
        std::cout << "IR Error: " << err << "\n";
    }
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*irModule);
    std::string moduleText = ss.str();

    CHECK(moduleText.find("sph_array_iter_create") != std::string::npos);
    CHECK(moduleText.find("sph_array_iter_valid") != std::string::npos);
    CHECK(moduleText.find("sph_array_iter_get_val") != std::string::npos);
    CHECK(moduleText.find("sph_array_iter_next") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for PHP class constructor, new expr, and object utilities") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class Person {\n"
        "    public $name;\n"
        "    public $age;\n"
        "    public function __construct($name, $age) {\n"
        "        $this->name = $name;\n"
        "        $this->age = $age;\n"
        "    }\n"
        "    public function greet(): string {\n"
        "        return \"Hello, \" . $this->name;\n"
        "    }\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let p = new Person(\"Alice\", 30);\n"
        "    let g = p->greet();\n"
        "    let isObj = is_object(p);\n"
        "    let cls = get_class(p);\n"
        "    let has = property_exists(p, \"name\");\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("php_obj_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "php_obj_ir_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    for (const auto& err : errors) {
        std::cout << "IR Error: " << err << "\n";
    }
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*irModule);
    std::string moduleText = ss.str();

    CHECK(moduleText.find("fn @Person___construct") != std::string::npos);
    CHECK(moduleText.find("fn @Person_greet") != std::string::npos);
    CHECK(moduleText.find("call void @Person___construct") != std::string::npos);
    CHECK(moduleText.find("call string @Person_greet") != std::string::npos);
    CHECK(moduleText.find("sph_is_object") != std::string::npos);
    CHECK(moduleText.find("sph_object_get_class") != std::string::npos);
    CHECK(moduleText.find("sph_property_exists") != std::string::npos);
}

TEST_CASE("IRGenerator generates verified IR for exceptions and by-reference parameters") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "function addOne(&$val: i32): void {\n"
        "    $val = $val + 1;\n"
        "}\n"
        "function runTest(): void {\n"
        "    try {\n"
        "        let mut x: i32 = 10;\n"
        "        addOne(x);\n"
        "        throw \"test exception\";\n"
        "    } catch ($e) {\n"
        "        echo $e;\n"
        "    } finally {\n"
        "        echo \"cleanup\";\n"
        "    }\n"
        "}\n";

    uint32_t fileId = sm.addSource("exceptions_ir.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "exceptions_ir_mod");
    CHECK(irModule != nullptr);

    sph::IRVerifier verifier;
    std::vector<std::string> errors;
    bool irOk = verifier.verify(*irModule, errors);
    for (const auto& err : errors) {
        std::cout << "IR Error: " << err << "\n";
    }
    CHECK(irOk);
    CHECK_EQ(errors.size(), 0u);

    std::ostringstream ss;
    sph::IRPrinter printer(ss);
    printer.print(*irModule);
    std::string moduleText = ss.str();

    CHECK(moduleText.find("setjmp") != std::string::npos);
    CHECK(moduleText.find("sph_exception_push_frame") != std::string::npos);
    CHECK(moduleText.find("sph_exception_pop_frame") != std::string::npos);
    CHECK(moduleText.find("sph_exception_throw") != std::string::npos);
    CHECK(moduleText.find("addressof") != std::string::npos);
}



