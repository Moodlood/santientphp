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
    return analyzeCode(code, emitter) && !emitter.hasErrors();
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
// Pattern Matching: Literals, Wildcards, Multi-patterns
// ============================================================================

TEST_CASE("PatternMatching: Matches integer literals and wildcard") {
    std::string code =
        "<?php\n"
        "fn describe(n: i32): string {\n"
        "    return match (n) {\n"
        "        0 => \"zero\",\n"
        "        1 => \"one\",\n"
        "        2 => \"two\",\n"
        "        _ => \"many\"\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("PatternMatching: Matches negative numbers") {
    std::string code =
        "<?php\n"
        "fn check_sign(n: i32): string {\n"
        "    return match (n) {\n"
        "        -1 => \"negative one\",\n"
        "        0 => \"zero\",\n"
        "        1 => \"one\",\n"
        "        _ => \"other\"\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("PatternMatching: Matches string literals") {
    std::string code =
        "<?php\n"
        "fn handle_cmd(cmd: string): i32 {\n"
        "    return match (cmd) {\n"
        "        \"start\" => 1,\n"
        "        \"stop\" => 2,\n"
        "        \"pause\" => 3,\n"
        "        _ => 0\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("PatternMatching: Matches boolean literals") {
    std::string code =
        "<?php\n"
        "fn bool_to_num(b: bool): i32 {\n"
        "    return match (b) {\n"
        "        true => 1,\n"
        "        false => 0\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("PatternMatching: Matches integer ranges 1..10") {
    std::string code =
        "<?php\n"
        "fn classify_score(score: i32): string {\n"
        "    return match (score) {\n"
        "        90..100 => \"A\",\n"
        "        80..89 => \"B\",\n"
        "        70..79 => \"C\",\n"
        "        _ => \"F\"\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("PatternMatching: Multi-patterns separated by pipe |") {
    std::string code =
        "<?php\n"
        "fn is_weekend(day: i32): bool {\n"
        "    return match (day) {\n"
        "        6 | 7 => true,\n"
        "        1 | 2 | 3 | 4 | 5 => false,\n"
        "        _ => false\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

// ============================================================================
// Pattern Matching: Variable Binding & Guards
// ============================================================================

TEST_CASE("PatternMatching: Variable binding pattern") {
    std::string code =
        "<?php\n"
        "fn abs_val(n: i32): i32 {\n"
        "    return match (n) {\n"
        "        0 => 0,\n"
        "        $x if $x > 0 => $x,\n"
        "        $x => -$x\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("PatternMatching: Match guards with if condition") {
    std::string code =
        "<?php\n"
        "fn filter_even(n: i32): string {\n"
        "    return match (n) {\n"
        "        $x if ($x % 2 == 0) => \"even\",\n"
        "        _ => \"odd\"\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

// ============================================================================
// Pattern Matching: Option & Result Destructuring
// ============================================================================

TEST_CASE("PatternMatching: Option Some($val) and None destructuring") {
    std::string code =
        "<?php\n"
        "fn unwrap_or_zero(opt: Option<i32>): i32 {\n"
        "    return match (opt) {\n"
        "        Some($val) => $val,\n"
        "        None => 0\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("PatternMatching: Result Ok($val) and Err($err) destructuring") {
    std::string code =
        "<?php\n"
        "fn handle_res(res: Result<i32, string>): string {\n"
        "    return match (res) {\n"
        "        Ok($v) => \"success\",\n"
        "        Err($e) => $e\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

TEST_CASE("PatternMatching: Rejects mismatched return types across match arms") {
    std::string code =
        "<?php\n"
        "fn bad_match(n: i32): i32 {\n"
        "    return match (n) {\n"
        "        1 => 100,\n"
        "        _ => \"string instead of i32\"\n"
        "    };\n"
        "}\n";
    CHECK(checkInvalid(code, "E0308"));
}

TEST_CASE("PatternMatching: Nested match expressions") {
    std::string code =
        "<?php\n"
        "fn nested(a: i32, b: bool): i32 {\n"
        "    return match (a) {\n"
        "        1 => match (b) {\n"
        "            true => 10,\n"
        "            false => 20\n"
        "        },\n"
        "        _ => 0\n"
        "    };\n"
        "}\n";
    CHECK(checkValid(code));
}

