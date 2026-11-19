#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"

namespace {

bool analyzeCode(const std::string& code, sph::DiagnosticEmitter& emitter) {
    sph::SourceManager sm;
    uint32_t fileId = sm.addSource("test.sph", code);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program || emitter.hasErrors()) {
        return false;
    }
    sph::SemanticAnalyzer analyzer(emitter);
    return analyzer.analyze(*program);
}

bool checkInvalid(const std::string& code) {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    bool ok = analyzeCode(code, emitter);
    return !ok || emitter.hasErrors();
}

bool checkValid(const std::string& code) {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    return analyzeCode(code, emitter) && !emitter.hasErrors();
}

} // namespace

// ============================================================================
// Negative Suite: Variable Declaration & Mutability
// ============================================================================

TEST_CASE("Negative: Cannot reassign immutable variable declared with let") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let x: i32 = 10;\n"
        "    x = 20;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Cannot declare duplicate variable in same scope") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let x: i32 = 10;\n"
        "    let x: i32 = 20;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Use of undeclared variable") {
    std::string code =
        "<?php\n"
        "fn main(): i32 {\n"
        "    return undefined_var;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Variable type annotation mismatch with initializer") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let x: i32 = \"string value\";\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Cannot assign string to boolean variable") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    mut b: bool = true;\n"
        "    b = \"hello\";\n"
        "}\n";
    CHECK(checkInvalid(code));
}

// ============================================================================
// Negative Suite: Function Return & Call Signatures
// ============================================================================

TEST_CASE("Negative: Returning value from void function") {
    std::string code =
        "<?php\n"
        "fn do_nothing(): void {\n"
        "    return 42;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Returning void from non-void function") {
    std::string code =
        "<?php\n"
        "fn get_val(): i32 {\n"
        "    return;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Missing return on all control paths") {
    std::string code =
        "<?php\n"
        "fn maybe_return(flag: bool): i32 {\n"
        "    if (flag) {\n"
        "        return 1;\n"
        "    }\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Calling function with too few arguments") {
    std::string code =
        "<?php\n"
        "fn add(a: i32, b: i32): i32 {\n"
        "    return a + b;\n"
        "}\n"
        "fn main(): void {\n"
        "    add(1);\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Calling function with too many arguments") {
    std::string code =
        "<?php\n"
        "fn add(a: i32, b: i32): i32 {\n"
        "    return a + b;\n"
        "}\n"
        "fn main(): void {\n"
        "    add(1, 2, 3);\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Calling function with argument type mismatch") {
    std::string code =
        "<?php\n"
        "fn print_num(a: i32): void {}\n"
        "fn main(): void {\n"
        "    print_num(\"not a number\");\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Calling non-existent function") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    ghost_function(123);\n"
        "}\n";
    CHECK(checkInvalid(code));
}

// ============================================================================
// Negative Suite: Structs & Fields
// ============================================================================

TEST_CASE("Negative: Accessing non-existent field on struct") {
    std::string code =
        "<?php\n"
        "struct Point { x: i32, y: i32 }\n"
        "fn main(): void {\n"
        "    let p = Point { x: 1, y: 2 };\n"
        "    let z = p.z;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Missing required field in struct initialization") {
    std::string code =
        "<?php\n"
        "struct Point { x: i32, y: i32 }\n"
        "fn main(): void {\n"
        "    let p = Point { x: 1 };\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Field type mismatch in struct initialization") {
    std::string code =
        "<?php\n"
        "struct Point { x: i32, y: i32 }\n"
        "fn main(): void {\n"
        "    let p = Point { x: \"one\", y: 2 };\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Mutating field of immutable struct binding") {
    std::string code =
        "<?php\n"
        "struct Point { x: i32, y: i32 }\n"
        "fn main(): void {\n"
        "    let p = Point { x: 1, y: 2 };\n"
        "    p.x = 10;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

// ============================================================================
// Negative Suite: Arrays & Slices
// ============================================================================

TEST_CASE("Negative: Array literal with mismatched element types") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let arr: [i32; 3] = [1, \"two\", 3];\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Array literal with wrong element count") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let arr: [i32; 3] = [1, 2];\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Indexing non-array non-slice type") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let num = 42;\n"
        "    let item = num[0];\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Indexing with non-integer index") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let arr = [1, 2, 3];\n"
        "    let item = arr[\"first\"];\n"
        "}\n";
    CHECK(checkInvalid(code));
}

// ============================================================================
// Negative Suite: Binary Operations & Types
// ============================================================================

TEST_CASE("Negative: Adding string and integer in static mode") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let res = \"hello\" + 5;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Bitwise AND on boolean and float") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let res = true & 3.14;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Logical NOT on struct") {
    std::string code =
        "<?php\n"
        "struct Foo {}\n"
        "fn main(): void {\n"
        "    let f = Foo {};\n"
        "    let not_f = !f;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Division by modulo with invalid operand types") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    let x = 10 % \"divisor\";\n"
        "}\n";
    CHECK(checkInvalid(code));
}

// ============================================================================
// Negative Suite: Control Flow & Unsafe Blocks
// ============================================================================

TEST_CASE("Negative: Break outside loop") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    break;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Continue outside loop") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    continue;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: While condition must evaluate to boolean") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    while (\"infinite\") {}\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: If condition must evaluate to boolean") {
    std::string code =
        "<?php\n"
        "fn main(): void {\n"
        "    if (42) {}\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Negative: Dereferencing raw pointer outside unsafe block") {
    std::string code =
        "<?php\n"
        "fn read_ptr(p: *i32): i32 {\n"
        "    return *p;\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Positive: Dereferencing raw pointer inside unsafe block is permitted") {
    std::string code =
        "<?php\n"
        "fn read_ptr(p: *i32): i32 {\n"
        "    unsafe {\n"
        "        return *p;\n"
        "    }\n"
        "}\n";
    CHECK(checkValid(code));
}

