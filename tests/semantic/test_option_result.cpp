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

bool checkValid(const std::string& code) {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    bool ok = analyzeCode(code, emitter);
    if (!ok || emitter.hasErrors()) {
        for (const auto& d : emitter.getDiagnostics()) {
            std::cerr << "checkValid FAILED: " << d.message << "\n";
        }
        return false;
    }
    return true;
}

bool checkInvalid(const std::string& code, const std::string& expectedErrorCode = "") {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    bool ok = analyzeCode(code, emitter);
    if (ok && !emitter.hasErrors()) {
        return false;
    }
    if (!expectedErrorCode.empty()) {
        for (const auto& diag : emitter.getDiagnostics()) {
            if (diag.code == expectedErrorCode || diag.message.find(expectedErrorCode) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    return true;
}

} // namespace

// ============================================================================
// Option<T> Type & Constructors
// ============================================================================

TEST_CASE("Option: SemanticAnalyzer accepts Option<i32> type annotation and Some") {
    std::string code =
        "<?php\n"
        "fn get_val(): Option<i32> {\n"
        "    return Some(42);\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("Option: SemanticAnalyzer accepts None return") {
    std::string code =
        "<?php\n"
        "fn get_empty(): Option<i32> {\n"
        "    return None;\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("Option: SemanticAnalyzer accepts Option::none() function call") {
    std::string code =
        "<?php\n"
        "fn get_empty(): Option<string> {\n"
        "    return Option::none();\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("Option: SemanticAnalyzer infers type from Some literal") {
    std::string code =
        "<?php\n"
        "fn test(): void {\n"
        "    let opt = Some(\"success\");\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("Option: SemanticAnalyzer rejects type mismatch in Some") {
    std::string code =
        "<?php\n"
        "fn get_num(): Option<i32> {\n"
        "    return Some(\"not a number\");\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Option: Nested Option<Option<bool>>") {
    std::string code =
        "<?php\n"
        "fn test(): Option<Option<bool>> {\n"
        "    return Some(Some(true));\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("Option: Option as struct field") {
    std::string code =
        "<?php\n"
        "struct User {\n"
        "    id: i32,\n"
        "    nickname: Option<string>,\n"
        "}\n"
        "fn make_user(): User {\n"
        "    return User {\n"
        "        id: 1,\n"
        "        nickname: Some(\"admin\"),\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("Option: Option in function parameter") {
    std::string code =
        "<?php\n"
        "fn process(opt: Option<i32>): i32 {\n"
        "    return 0;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    process(Some(10));\n"
        "    process(None);\n"
        "    return 0;\n"
        "}\n";
    CHECK(checkValid(code));
}

// ============================================================================
// Result<T, E> Type & Constructors
// ============================================================================

TEST_CASE("Result: SemanticAnalyzer accepts Result<i32, string> Ok return") {
    std::string code =
        "<?php\n"
        "fn divide(a: i32, b: i32): Result<i32, string> {\n"
        "    if (b == 0) {\n"
        "        return Err(\"division by zero\");\n"
        "    }\n"
        "    return Ok(a / b);\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("Result: SemanticAnalyzer accepts Result::ok and Result::err") {
    std::string code =
        "<?php\n"
        "fn test(): Result<bool, i32> {\n"
        "    return Result::ok(true);\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("Result: Rejects mismatch in Ok payload") {
    std::string code =
        "<?php\n"
        "fn test(): Result<i32, string> {\n"
        "    return Ok(\"wrong\");\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Result: Rejects mismatch in Err payload") {
    std::string code =
        "<?php\n"
        "fn test(): Result<i32, string> {\n"
        "    return Err(404);\n"
        "}\n";
    CHECK(checkInvalid(code));
}

TEST_CASE("Result: Struct with Result field") {
    std::string code =
        "<?php\n"
        "struct ApiResponse {\n"
        "    statusCode: i32,\n"
        "    data: Result<string, string>,\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("Result: Result containing Option") {
    std::string code =
        "<?php\n"
        "fn find_user(id: i32): Result<Option<string>, string> {\n"
        "    if (id < 0) {\n"
        "        return Err(\"invalid id\");\n"
        "    }\n"
        "    return Ok(Some(\"Alice\"));\n"
        "}\n";
    CHECK(checkValid(code));
}

// ============================================================================
// '?' Try Operator
// ============================================================================

TEST_CASE("TryOperator: Propagates Result in Result-returning function") {
    std::string code =
        "<?php\n"
        "fn helper(): Result<i32, string> {\n"
        "    return Ok(10);\n"
        "}\n"
        "fn run(): Result<i32, string> {\n"
        "    let val = helper()?;\n"
        "    return Ok(val + 5);\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("TryOperator: Propagates Option in Option-returning function") {
    std::string code =
        "<?php\n"
        "fn helper(): Option<i32> {\n"
        "    return Some(20);\n"
        "}\n"
        "fn run(): Option<i32> {\n"
        "    let val = helper()?;\n"
        "    return Some(val * 2);\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("TryOperator: Rejects '?' on non-Result non-Option type") {
    std::string code =
        "<?php\n"
        "fn run(): i32 {\n"
        "    let x = 42;\n"
        "    let y = x?;\n"
        "    return y;\n"
        "}\n";
    CHECK(checkInvalid(code, "E0277"));
}

TEST_CASE("TryOperator: Rejects '?' on Result when enclosing function returns non-Result") {
    std::string code =
        "<?php\n"
        "fn helper(): Result<i32, string> {\n"
        "    return Ok(10);\n"
        "}\n"
        "fn run(): i32 {\n"
        "    let val = helper()?;\n"
        "    return val;\n"
        "}\n";
    CHECK(checkInvalid(code, "E0277"));
}

