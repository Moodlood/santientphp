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
