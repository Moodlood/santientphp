#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"

TEST_CASE("SemanticAnalyzer accepts SPH 0.1 acceptance program") {
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

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(ok);
    CHECK(!emitter.hasErrors());
    CHECK_EQ(emitter.getErrorCount(), 0u);
}

TEST_CASE("SemanticAnalyzer infers variable types deterministically") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): void {\n"
        "    let intVal = 42;\n"
        "    let floatVal = 3.14;\n"
        "    let strVal = \"hello\";\n"
        "    let boolVal = true;\n"
        "}\n";

    uint32_t fileId = sm.addSource("infer.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(ok);
    CHECK(!emitter.hasErrors());

    const auto& stmts = program->getFunctions()[0]->getBody().getStatements();
    auto* v1 = dynamic_cast<sph::VarDeclStmt*>(stmts[0].get());
    auto* v2 = dynamic_cast<sph::VarDeclStmt*>(stmts[1].get());
    auto* v3 = dynamic_cast<sph::VarDeclStmt*>(stmts[2].get());
    auto* v4 = dynamic_cast<sph::VarDeclStmt*>(stmts[3].get());

    CHECK(v1->getInitializer().getInferredType()->isI32());
    CHECK(v2->getInitializer().getInferredType()->isF64());
    CHECK(v3->getInitializer().getInferredType()->isString());
    CHECK(v4->getInitializer().getInferredType()->isBoolean());
}

TEST_CASE("SemanticAnalyzer rejects assignment to immutable let binding") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): void {\n"
        "    let x = 10;\n"
        "    x = 20;\n"
        "}\n";

    uint32_t fileId = sm.addSource("immutable.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
    CHECK_EQ(emitter.getErrorCount(), 1u);
}

TEST_CASE("SemanticAnalyzer allows assignment to mutable mut binding") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): void {\n"
        "    mut x = 10;\n"
        "    x = 20;\n"
        "}\n";

    uint32_t fileId = sm.addSource("mutable.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer detects duplicate variable in same scope") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): void {\n"
        "    let a = 1;\n"
        "    let a = 2;\n"
        "}\n";

    uint32_t fileId = sm.addSource("dup_var.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
    CHECK_EQ(emitter.getErrorCount(), 1u);
}

TEST_CASE("SemanticAnalyzer allows variable shadowing in inner scope") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): void {\n"
        "    let a = 1;\n"
        "    {\n"
        "        let a = 2;\n"
        "    }\n"
        "}\n";

    uint32_t fileId = sm.addSource("shadow.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects undeclared variable access") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): void {\n"
        "    let a = unknownVar + 1;\n"
        "}\n";

    uint32_t fileId = sm.addSource("undeclared.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts valid array slicing and slice length access") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn take(s: &[i32]): i32 {\n"
        "    return s.len + s[0];\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let arr: [i32; 4] = [10, 20, 30, 40];\n"
        "    let s1: &[i32] = arr[1..3];\n"
        "    let s2 = s1[..1];\n"
        "    return take(s2);\n"
        "}\n";
    uint32_t fileId = sm.addSource("valid_slice.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects slicing non-array and non-slice types") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let x: i32 = 42;\n"
        "    let bad = x[0..1];\n"
        "}\n";
    uint32_t fileId = sm.addSource("bad_slice.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer type-checks File, Process, and Env standard library APIs") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let content: string = File::read(\"input.txt\");\n"
        "    let ok1: bool = File::write(\"output.txt\", content);\n"
        "    let ok2: bool = File::append(\"output.txt\", \"more\");\n"
        "    let ex: bool = File::exists(\"output.txt\");\n"
        "    let del: bool = File::delete(\"output.txt\");\n"
        "    let val: string = Env::get(\"PATH\");\n"
        "    let ok_env: bool = Env::set(\"KEY\", \"VAL\");\n"
        "    Process::exit(0);\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("file_env_valid.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects invalid argument types and counts for File and Env APIs") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let a = File::read(123);\n"
        "    let b = File::write(\"f.txt\");\n"
        "    let c = Process::exit(\"bad\");\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("file_invalid.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer type-checks Result types, Ok, Err, and unwrap") {
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
        "    let isOk: bool = r.is_ok();\n"
        "    let isErr: bool = r.is_err();\n"
        "    let val: i32 = r.unwrap();\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("result_valid.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects invalid Result operations") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let x: i32 = 42;\n"
        "    let u = x.unwrap();\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("result_invalid.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer type-checks valid Arena operations") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Node { value: i32 }\n"
        "fn main(): i32 {\n"
        "    let mut a: Arena = Arena::new(1024);\n"
        "    let p1: *Node = Arena::alloc(a, Node { value: 42 });\n"
        "    let p2: *Node = a.alloc(Node { value: 99 });\n"
        "    a.reset();\n"
        "    Arena::reset(a);\n"
        "    a.destroy();\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("arena_valid.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects invalid capacity in Arena::new") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let a = Arena::new(\"invalid\");\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("arena_invalid_cap.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects Arena::alloc with non-Arena first argument") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let x: i32 = 42;\n"
        "    let p = Arena::alloc(x, 100);\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("arena_invalid_alloc.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects use of Arena after destroy") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let mut a: Arena = Arena::new(1024);\n"
        "    a.destroy();\n"
        "    a.reset();\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("arena_use_after_destroy.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer type-checks string concatenation, equality, and len method") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let a: string = \"foo\";\n"
        "    let b: string = \"bar\";\n"
        "    let c: string = a + b;\n"
        "    let l: i64 = c.len();\n"
        "    let eq: bool = (a == b);\n"
        "    let neq: bool = (a != b);\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("string_valid.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer type-checks valid Vec operations") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let mut v: Vec<i32> = Vec::new();\n"
        "    v.push(10);\n"
        "    let l: i64 = v.len();\n"
        "    let cap: i64 = v.capacity();\n"
        "    let x: i32 = v[0];\n"
        "    v[0] = 42;\n"
        "    let popped: i32 = v.pop();\n"
        "    v.destroy();\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("vec_valid.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects type mismatch in Vec::push") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let mut v: Vec<i32> = Vec::new();\n"
        "    v.push(\"wrong\");\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("vec_type_mismatch.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects use of Vec after destroy") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let mut v: Vec<i32> = Vec::new();\n"
        "    v.destroy();\n"
        "    v.push(10);\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("vec_use_after_destroy.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer type-checks valid Box operations and move semantics") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Node { value: i32 }\n"
        "fn main(): i32 {\n"
        "    let b1: Box<Node> = Box::new(Node { value: 42 });\n"
        "    let v1: i32 = b1.value;\n"
        "    let b2: Box<Node> = b1;\n"
        "    let v2: i32 = b2.value;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("box_valid.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects use of moved value after assignment") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Node { value: i32 }\n"
        "fn main(): i32 {\n"
        "    let b1: Box<Node> = Box::new(Node { value: 42 });\n"
        "    let b2: Box<Node> = b1;\n"
        "    let v = b1.value;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("box_moved_var.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects use of moved value after function call") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Node { value: i32 }\n"
        "fn consume(n: Box<Node>): i32 {\n"
        "    return n.value;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let b1: Box<Node> = Box::new(Node { value: 42 });\n"
        "    let r = consume(b1);\n"
        "    let v = b1.value;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("box_moved_fn.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer allows reassignment to moved mutable variable") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Node { value: i32 }\n"
        "fn main(): i32 {\n"
        "    let mut b1: Box<Node> = Box::new(Node { value: 10 });\n"
        "    let b2: Box<Node> = b1;\n"
        "    b1 = Box::new(Node { value: 20 });\n"
        "    let v = b1.value;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("box_reassign.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer calculates packed struct layout without padding") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "#[repr(packed)]\n"
        "struct PackedData {\n"
        "    a: i32,\n"
        "    b: bool,\n"
        "    c: i32,\n"
        "}\n"
        "struct StandardData {\n"
        "    a: i32,\n"
        "    b: bool,\n"
        "    c: i32,\n"
        "}\n";

    uint32_t fileId = sm.addSource("packed_layout_sem.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts valid bitwise and shift operations on integers") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let a: i32 = 12;\n"
        "    let b: i32 = 10;\n"
        "    let c: i32 = a & b;\n"
        "    let d: i32 = a | b;\n"
        "    let e: i32 = a ^ b;\n"
        "    let f: i32 = a << 1;\n"
        "    let g: i32 = a >> 1;\n"
        "    let h: i32 = ~a;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("bitwise_ok.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects bitwise operations on non-integers") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let a: f64 = 1.5;\n"
        "    let b: f64 = 2.5;\n"
        "    let c = a & b;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("bitwise_err.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts pointer arithmetic inside unsafe block") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    mut arr: [i32; 3] = [1, 2, 3];\n"
        "    unsafe {\n"
        "        let p: *i32 = &arr[0];\n"
        "        let p2: *i32 = p + 1;\n"
        "        let p3: *i32 = 2 + p;\n"
        "        let p4: *i32 = p2 - 1;\n"
        "        let diff: i64 = p3 - p2;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("ptr_arith_ok.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects pointer arithmetic outside unsafe block") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    mut arr: [i32; 3] = [1, 2, 3];\n"
        "    let p: *i32 = &arr[0];\n"
        "    let p2: *i32 = p + 1;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("ptr_arith_err.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts valid type casts and rejects invalid ones") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let a: i32 = 42;\n"
        "    let b: i64 = a as i64;\n"
        "    let c: i32 = b as i32;\n"
        "    let d: f64 = a as f64;\n"
        "    let e: i32 = d as i32;\n"
        "    unsafe {\n"
        "        let p: *i32 = &a;\n"
        "        let addr: i64 = p as i64;\n"
        "        let p2: *i32 = addr as *i32;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("cast_ok.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects integer-to-pointer cast outside unsafe block") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let addr: i64 = 1000;\n"
        "    let p: *i32 = addr as *i32;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("cast_unsafe_err.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts valid extern C function declarations and calls") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "extern \"C\" fn abs(n: i32): i32;\n"
        "extern \"C\" fn puts(s: string): i32;\n"
        "fn main(): i32 {\n"
        "    let x: i32 = -10;\n"
        "    let y: i32 = abs(x);\n"
        "    puts(\"testing\");\n"
        "    return y;\n"
        "}\n";
    uint32_t fileId = sm.addSource("extern_ok.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects invalid FFI parameter types in extern function") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Point { x: i32, y: i32 }\n"
        "extern \"C\" fn processPoint(p: Point): void;\n";
    uint32_t fileId = sm.addSource("extern_bad_ffi.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects duplicate extern function definition") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "extern \"C\" fn abs(n: i32): i32;\n"
        "extern \"C\" fn abs(n: i32): i32;\n";
    uint32_t fileId = sm.addSource("extern_dup.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer monomorphizes generic structs and generic functions") {
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
        "fn main(): i32 {\n"
        "    let p = makePair<i32, i32>(10, 20);\n"
        "    let sum = add<i32>(p.first, p.second);\n"
        "    return sum;\n"
        "}\n";
    uint32_t fileId = sm.addSource("generics_ok.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects generic call with type argument count mismatch") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn add<T>(a: T, b: T): T { return a + b; }\n"
        "fn main(): void {\n"
        "    let x = add<i32, bool>(1, 2);\n"
        "}\n";
    uint32_t fileId = sm.addSource("gen_arg_mismatch.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects generic function call with argument type mismatch") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn add<T>(a: T, b: T): T { return a + b; }\n"
        "fn main(): void {\n"
        "    let x = add<i32>(true, 2);\n"
        "}\n";
    uint32_t fileId = sm.addSource("gen_call_mismatch.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects generic struct initialization with field type mismatch") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Pair<T, U> { first: T, second: U }\n"
        "fn main(): void {\n"
        "    let p = Pair<i32, i32> { first: true, second: 20 };\n"
        "}\n";
    uint32_t fileId = sm.addSource("gen_struct_mismatch.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts valid pointer operations inside unsafe block") {
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
    uint32_t fileId = sm.addSource("ptr_valid.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects raw pointer dereference outside unsafe block") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn readPtr(p: *i32): i32 {\n"
        "    let v = *p;\n"
        "    return v;\n"
        "}\n";
    uint32_t fileId = sm.addSource("ptr_no_unsafe_read.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects raw pointer dereference assignment outside unsafe block") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn writePtr(p: *i32): void {\n"
        "    *p = 100;\n"
        "}\n";
    uint32_t fileId = sm.addSource("ptr_no_unsafe_write.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects taking address of non-lvalue expression") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let p = &42;\n"
        "}\n";
    uint32_t fileId = sm.addSource("ptr_addr_non_lvalue.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects dereferencing non-pointer type") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let x: i32 = 42;\n"
        "    unsafe {\n"
        "        let y = *x;\n"
        "    }\n"
        "}\n";
    uint32_t fileId = sm.addSource("ptr_deref_non_ptr.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects type mismatch in pointer dereference assignment") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn writePtr(p: *i32): void {\n"
        "    unsafe {\n"
        "        *p = true;\n"
        "    }\n"
        "}\n";
    uint32_t fileId = sm.addSource("ptr_type_mismatch.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects type mismatch in initialization") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): void {\n"
        "    let a: i32 = \"not_an_int\";\n"
        "}\n";

    uint32_t fileId = sm.addSource("type_mismatch.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects non-boolean condition in if and while") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn test(): void {\n"
        "    if (123) {}\n"
        "    while (\"loop\") {}\n"
        "}\n";

    uint32_t fileId = sm.addSource("bad_cond.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
    CHECK_EQ(emitter.getErrorCount(), 2u);
}

TEST_CASE("SemanticAnalyzer rejects missing return in non-void function") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn calculate(): i32 {\n"
        "    let x = 10;\n"
        "}\n";

    uint32_t fileId = sm.addSource("missing_ret.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects return type mismatch") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn getNum(): i32 {\n"
        "    return \"string_value\";\n"
        "}\n";

    uint32_t fileId = sm.addSource("ret_mismatch.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts valid struct declaration and usage") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "fn main(): i32 {\n"
        "    mut p = Point { x: 10, y: 20 };\n"
        "    p.x = 30;\n"
        "    return p.x + p.y;\n"
        "}\n";

    uint32_t fileId = sm.addSource("valid_struct.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects assignment to member of immutable struct") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "fn main(): void {\n"
        "    let p = Point { x: 10, y: 20 };\n"
        "    p.x = 30;\n"
        "}\n";

    uint32_t fileId = sm.addSource("immutable_struct.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects missing field in struct initialization") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "fn main(): void {\n"
        "    let p = Point { x: 10 };\n"
        "}\n";

    uint32_t fileId = sm.addSource("missing_field.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects unknown member access") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "fn main(): void {\n"
        "    let p = Point { x: 10, y: 20 };\n"
        "    let z = p.nonexistent;\n"
        "}\n";

    uint32_t fileId = sm.addSource("unknown_field.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts valid array declarations, indexing, and element mutation") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let mut arr: [i32; 3] = [10, 20, 30];\n"
        "    arr[1] = 40;\n"
        "    let val = arr[1];\n"
        "    return val;\n"
        "}\n";

    uint32_t fileId = sm.addSource("array_valid.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects indexing non-array types") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let x = 10;\n"
        "    let y = x[0];\n"
        "}\n";

    uint32_t fileId = sm.addSource("not_array.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects non-integer index") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let arr = [1, 2, 3];\n"
        "    let y = arr[\"index\"];\n"
        "}\n";

    uint32_t fileId = sm.addSource("invalid_index.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects assignment to element of immutable array") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let arr = [10, 20, 30];\n"
        "    arr[0] = 50;\n"
        "}\n";

    uint32_t fileId = sm.addSource("immutable_array.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects array size mismatch") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let arr: [i32; 3] = [10, 20];\n"
        "}\n";

    uint32_t fileId = sm.addSource("size_mismatch.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts valid enum declarations and equality comparisons") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "enum Status {\n"
        "    Pending,\n"
        "    Active,\n"
        "}\n"
        "fn checkStatus(s: Status): bool {\n"
        "    return s == Status::Active;\n"
        "}\n"
        "fn main(): void {\n"
        "    let s = Status::Pending;\n"
        "    let res = checkStatus(s);\n"
        "}\n";

    uint32_t fileId = sm.addSource("enum_valid.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects duplicate enum declaration") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "enum Status { A, B }\n"
        "enum Status { C, D }\n";

    uint32_t fileId = sm.addSource("dup_enum.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects duplicate variant in enum") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "enum Status { A, A }\n";

    uint32_t fileId = sm.addSource("dup_variant.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects unknown enum or variant in scoped access") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "enum Status { A, B }\n"
        "fn main(): void {\n"
        "    let x = Status::NonExistent;\n"
        "}\n";

    uint32_t fileId = sm.addSource("unknown_variant.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects comparison between enum and integer") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "enum Status { A, B }\n"
        "fn main(): void {\n"
        "    let s = Status::A;\n"
        "    let bad = (s == 0);\n"
        "}\n";

    uint32_t fileId = sm.addSource("enum_int_cmp.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer resolves functions, structs and enums across imported modules") {
    sph::SourceManager sm;
    std::string typesCode =
        "<?php\n"
        "struct Point { x: i32, y: i32 }\n"
        "enum Mode { Fast, Safe }\n"
        "fn makePoint(x: i32, y: i32): Point {\n"
        "    return Point { x: x, y: y };\n"
        "}\n";
    (void)sm.addSource("types.sph", typesCode);

    std::string mainCode =
        "<?php\n"
        "import \"types.sph\";\n"
        "fn main(): i32 {\n"
        "    let p = makePoint(10, 20);\n"
        "    let m = Mode::Fast;\n"
        "    if (m == Mode::Fast) {\n"
        "        return p.x + p.y;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    uint32_t mainFileId = sm.addSource("main.sph", mainCode);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(mainFileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects duplicate function collision across imported modules") {
    sph::SourceManager sm;
    std::string modCode =
        "<?php\n"
        "fn calculate(): i32 { return 1; }\n";
    (void)sm.addSource("mod.sph", modCode);

    std::string mainCode =
        "<?php\n"
        "import \"mod.sph\";\n"
        "fn calculate(): i32 { return 2; }\n";
    uint32_t mainFileId = sm.addSource("main.sph", mainCode);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(mainFileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);

    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts valid class, fields, methods, this, and method calls") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class BankAccount {\n"
        "    balance: i32,\n"
        "    fn deposit(amount: i32): void {\n"
        "        this.balance = this.balance + amount;\n"
        "    }\n"
        "    fn getBalance(): i32 {\n"
        "        return this.balance;\n"
        "    }\n"
        "}\n"
        "fn main(): i32 {\n"
        "    mut acc = BankAccount { balance: 100 };\n"
        "    acc.deposit(50);\n"
        "    return acc.getBalance();\n"
        "}\n";
    uint32_t fileId = sm.addSource("valid_class.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects this outside of class method") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let x = this;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("bad_this.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects calling mutating method on immutable binding") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class Counter {\n"
        "    count: i32,\n"
        "    fn inc(): void {\n"
        "        this.count = this.count + 1;\n"
        "    }\n"
        "}\n"
        "fn main(): void {\n"
        "    let c = Counter { count: 0 };\n"
        "    c.inc();\n"
        "}\n";
    uint32_t fileId = sm.addSource("immut_class.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects unknown method or method argument mismatch") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class Counter {\n"
        "    count: i32,\n"
        "    fn inc(step: i32): void {\n"
        "        this.count = this.count + step;\n"
        "    }\n"
        "}\n"
        "fn main(): void {\n"
        "    mut c = Counter { count: 0 };\n"
        "    c.unknownMethod();\n"
        "    c.inc(1, 2);\n"
        "}\n";
    uint32_t fileId = sm.addSource("bad_method.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts class implementing interface and passing to interface parameter") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "interface Greeter {\n"
        "    fn greet(): i32;\n"
        "}\n"
        "class FriendlyGreeter implements Greeter {\n"
        "    code: i32,\n"
        "    fn greet(): i32 {\n"
        "        return this.code;\n"
        "    }\n"
        "}\n"
        "fn runGreeter(g: Greeter): i32 {\n"
        "    return g.greet();\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let fg = FriendlyGreeter { code: 42 };\n"
        "    return runGreeter(fg);\n"
        "}\n";
    uint32_t fileId = sm.addSource("iface_accept.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects class missing interface method") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "interface Drawable {\n"
        "    fn draw(): void;\n"
        "}\n"
        "class Incomplete implements Drawable {\n"
        "    id: i32,\n"
        "}\n"
        "fn main(): void {}\n";
    uint32_t fileId = sm.addSource("missing_method.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects class method signature mismatch with interface") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "interface Calculator {\n"
        "    fn calc(a: i32, b: i32): i32;\n"
        "}\n"
        "class BadCalc implements Calculator {\n"
        "    fn calc(a: i32): i32 { return a; }\n"
        "}\n"
        "fn main(): void {}\n";
    uint32_t fileId = sm.addSource("bad_sig.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects assigning class not implementing interface") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "interface Runner {\n"
        "    fn run(): void;\n"
        "}\n"
        "class NonRunner {\n"
        "    speed: i32,\n"
        "}\n"
        "fn execute(r: Runner): void {}\n"
        "fn main(): void {\n"
        "    let nr = NonRunner { speed: 10 };\n"
        "    execute(nr);\n"
        "}\n";
    uint32_t fileId = sm.addSource("not_implementing.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer type-checks valid Rc operations and auto-dereference") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "struct Point { x: i32, y: i32 }\n"
        "fn main(): i32 {\n"
        "    let p: Rc<Point> = Rc::new(Point { x: 10, y: 20 });\n"
        "    let q: Rc<Point> = p;\n"
        "    let px: i32 = p.x;\n"
        "    let count: i64 = Rc::count(q);\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("rc_valid.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects Rc::count on non-Rc type") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    let x: i32 = 42;\n"
        "    let c = Rc::count(x);\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("rc_invalid_count.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer allows dynamic variable declarations and reassignments in dynamic mode") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    $x = 42;\n"
        "    $x = \"hello world\";\n"
        "    $msg = $x . \"!\";\n"
        "    echo $msg;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("dynamic_mode.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}
TEST_CASE("SemanticAnalyzer analyzes foreach loops and loop control statements") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    $arr = [1, 2, 3];\n"
        "    foreach ($arr as $k => $v) {\n"
        "        if ($v == 2) {\n"
        "            continue;\n"
        "        }\n"
        "        break;\n"
        "    }\n"
        "    let fixed: [i32; 3] = [10, 20, 30];\n"
        "    foreach ($fixed as $elem) {\n"
        "        echo $elem;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("foreach_sem.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts PHP class with constructor, new expression, $this, and method calls") {
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
        "fn main(): void {\n"
        "    let p = new Person(\"Alice\", 30);\n"
        "    let g = p->greet();\n"
        "}\n";
    uint32_t fileId = sm.addSource("php_class_sem.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer validates is_object, get_class, and property_exists built-ins") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class Point {\n"
        "    public $x;\n"
        "    public $y;\n"
        "}\n"
        "fn main(): void {\n"
        "    let pt = new Point();\n"
        "    let isObj = is_object(pt);\n"
        "    let clsName = get_class(pt);\n"
        "    let hasX = property_exists(pt, \"x\");\n"
        "}\n";
    uint32_t fileId = sm.addSource("php_obj_utils_sem.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects new expression with argument count mismatch") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class BoxItem {\n"
        "    public $val;\n"
        "    public function __construct($val) {\n"
        "        $this->val = $val;\n"
        "    }\n"
        "}\n"
        "fn main(): void {\n"
        "    let b = new BoxItem(1, 2);\n"
        "}\n";
    uint32_t fileId = sm.addSource("new_mismatch.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects new expression for unknown class") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let u = new NonExistentClass();\n"
        "}\n";
    uint32_t fileId = sm.addSource("new_unknown.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects break and continue outside loops") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    break;\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("break_outside.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer validates PHP standard library core built-ins") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    $arr = [\"apple\", \"banana\"];\n"
        "    let c = count($arr);\n"
        "    let isArr = is_array($arr);\n"
        "    let isStr = is_string(\"test\");\n"
        "    let isInt = is_int(42);\n"
        "    let len = strlen(\"hello\");\n"
        "    let sub = substr(\"hello\", 1, 3);\n"
        "    let pos = strpos(\"hello\", \"ll\");\n"
        "    let parts = explode(\",\", \"a,b,c\");\n"
        "    let joined = implode(\"-\", parts);\n"
        "    return 0;\n"
        "}\n";
    uint32_t fileId = sm.addSource("stdlib_sem.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer accepts PHP class with constructor, new expression, $this, and method calls") {
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
        "fn main(): void {\n"
        "    let p = new Person(\"Alice\", 30);\n"
        "    let g = p->greet();\n"
        "}\n";
    uint32_t fileId = sm.addSource("php_class_sem.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer validates is_object, get_class, and property_exists built-ins") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class Point {\n"
        "    public $x;\n"
        "    public $y;\n"
        "}\n"
        "fn main(): void {\n"
        "    let pt = new Point();\n"
        "    let isObj = is_object(pt);\n"
        "    let clsName = get_class(pt);\n"
        "    let hasX = property_exists(pt, \"x\");\n"
        "}\n";
    uint32_t fileId = sm.addSource("php_obj_utils_sem.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects new expression with argument count mismatch") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "class BoxItem {\n"
        "    public $val;\n"
        "    public function __construct($val) {\n"
        "        $this->val = $val;\n"
        "    }\n"
        "}\n"
        "fn main(): void {\n"
        "    let b = new BoxItem(1, 2);\n"
        "}\n";
    uint32_t fileId = sm.addSource("new_mismatch.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer rejects new expression for unknown class") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let u = new NonExistentClass();\n"
        "}\n";
    uint32_t fileId = sm.addSource("new_unknown.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(!ok);
    CHECK(emitter.hasErrors());
}

TEST_CASE("SemanticAnalyzer validates SIMD vector types, constructors, operators, and CPU feature queries (Milestone 11C)") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "function test_simd(): void {\n"
        "    let hasSse: bool = Simd::has_sse();\n"
        "    let hasAvx: bool = Simd::has_avx();\n"
        "    let v1 = new f32x4(1.0, 2.0, 3.0, 4.0);\n"
        "    let v2 = f32x4::splat(5.0);\n"
        "    let v3 = v1 + v2;\n"
        "    let s: f32 = v3.sum();\n"
        "    let d: f32 = v1.dot(v2);\n"
        "    let iv = new i32x4(10, 20, 30, 40);\n"
        "    let iv2 = iv + iv;\n"
        "    let isum: i32 = iv2.sum();\n"
        "    let dv = new f64x2(100.0, 200.0);\n"
        "    let dsum: f64 = dv.sum();\n"
        "    let lv = new i64x2(1000, 2000);\n"
        "    let lsum: i64 = lv.sum();\n"
        "}\n";
    uint32_t fileId = sm.addSource("simd_semantic.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);

    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    CHECK(ok);
    CHECK(!emitter.hasErrors());
}

