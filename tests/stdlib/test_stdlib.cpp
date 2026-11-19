#include "common/test_framework.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <sstream>

TEST_CASE("Stdlib: import std/core.sph and invoke utility functions") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/core.sph\";\n"
        "\n"
        "fn main(): i32 {\n"
        "    assert(true, \"ok\");\n"
        "    let m: i32 = min_i32(10, 20);\n"
        "    let mx: i32 = max_i32(10, 20);\n"
        "    let ab: i32 = abs_i32(-42);\n"
        "    let cl: i32 = clamp_i32(50, 0, 100);\n"
        "    return m + mx + ab + cl;\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_core.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "test_core_module");
    CHECK(irModule != nullptr);
}

TEST_CASE("Stdlib: import std/io.sph and std/fs.sph") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/io.sph\";\n"
        "import \"std/fs.sph\";\n"
        "\n"
        "fn main(): i32 {\n"
        "    print_str(\"Testing stdlib IO and FS\\n\");\n"
        "    let exists: bool = file_exists(\"nonexistent.txt\");\n"
        "    if (exists) {\n"
        "        println_str(\"File exists\");\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_io_fs.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));
}

TEST_CASE("Stdlib: import std/collections.sph and std/async.sph") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/collections.sph\";\n"
        "import \"std/async.sph\";\n"
        "\n"
        "fn main(): i32 {\n"
        "    let r: IntRange = range(0, 10);\n"
        "    let len: i32 = range_length(r);\n"
        "    yield_now();\n"
        "    return len;\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_col_async.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "test_col_module");
    CHECK(irModule != nullptr);

    std::ostringstream ss;
    sph::X86_64CodeGen codegen(ss);
    codegen.generate(*irModule);
    CHECK(!ss.str().empty());
}

TEST_CASE("Stdlib: import std/net.sph with http_get signature") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/net.sph\";\n"
        "\n"
        "fn main(): i32 {\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_net.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));
}

TEST_CASE("Stdlib: import std/crypto.sph passes check and IR generation") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/crypto.sph\";\n"
        "\n"
        "fn main(): i32 {\n"
        "    let h = crypto_sha256(\"test string\");\n"
        "    let rnd = crypto_random_u32();\n"
        "    if (rnd != 0) { return 0; }\n"
        "    return 1;\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_crypto.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "test_crypto_module");
    CHECK(irModule != nullptr);
}

TEST_CASE("Stdlib: import std/json.sph passes check and IR generation") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/json.sph\";\n"
        "\n"
        "fn main(): i32 {\n"
        "    let valid = json_validate(\"{\\\"name\\\":\\\"Alice\\\"}\");\n"
        "    let s = json_escape(\"foo\");\n"
        "    let pair = json_stringify_pair(\"foo\", \"bar\");\n"
        "    let obj = json_wrap_object(pair);\n"
        "    if (valid) { return 0; }\n"
        "    return 1;\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_json.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "test_json_module");
    CHECK(irModule != nullptr);
}

TEST_CASE("Stdlib: import std/time.sph passes check and IR generation") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/time.sph\";\n"
        "\n"
        "fn main(): i32 {\n"
        "    let n = instant_now();\n"
        "    let ms = instant_elapsed_ms(n);\n"
        "    let us = instant_elapsed_micros(n);\n"
        "    let dur = duration_from_millis(100);\n"
        "    if (ms >= 0) { return 0; }\n"
        "    return 1;\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_time.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "test_time_module");
    CHECK(irModule != nullptr);
}

TEST_CASE("Stdlib: All 12 standard modules compile and check cleanly") {
    std::vector<std::string> modules = {
        "async.sph", "collections.sph", "core.sph", "crypto.sph",
        "fs.sph", "gui.sph", "io.sph", "json.sph",
        "net.sph", "process.sph", "time.sph", "web.sph"
    };

    for (const auto& mod : modules) {
        sph::SourceManager sm;
        std::filesystem::path stdPath = std::filesystem::path("stdlib") / mod;
        if (!std::filesystem::exists(stdPath) && std::filesystem::exists(std::filesystem::path("../stdlib") / mod)) {
            stdPath = std::filesystem::path("../stdlib") / mod;
        }
        CHECK(std::filesystem::exists(stdPath));
        auto fileIdOpt = sm.loadFile(stdPath.string());
        CHECK(fileIdOpt.has_value());
        if (!fileIdOpt.has_value()) continue;
        uint32_t fileId = *fileIdOpt;
        sph::DiagnosticEmitter emitter(sm);
        sph::Parser parser(fileId, sm, &emitter);
        auto program = parser.parseProgram();
        CHECK(!emitter.hasErrors());
        CHECK(program != nullptr);

        sph::SemanticAnalyzer analyzer(emitter);
        CHECK(analyzer.analyze(*program));
    }
}
