#include "common/test_framework.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include "runtime/core/core.h"
#include "runtime/async/async.h"
#include "runtime/net/websocket.h"
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

TEST_CASE("Stdlib: All standard modules compile and check cleanly") {
    std::vector<std::string> modules = {
        "async.sph", "buffer.sph", "collections.sph", "core.sph", "crypto.sph",
        "fs.sph", "gui.sph", "io.sph", "json.sph",
        "net.sph", "process.sph", "thread.sph", "time.sph", "web.sph"
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

TEST_CASE("Stdlib P1: ByteBuffer runtime binary operations and null-byte preservation") {
    uint8_t raw_payload[8] = { 0x42, 0x00, 0x50, 0x00, 0x48, 0x00, 0xFF, 0x00 };
    auto* buf = sph_buffer_from_raw(raw_payload, 8);
    CHECK(buf != nullptr);
    CHECK_EQ(sph_buffer_len(buf), 8);
    CHECK(sph_buffer_cap(buf) >= 8);

    uint8_t* ptr = sph_buffer_ptr(buf);
    CHECK(ptr != nullptr);
    CHECK_EQ(ptr[0], 0x42);
    CHECK_EQ(ptr[1], 0x00);
    CHECK_EQ(ptr[2], 0x50);
    CHECK_EQ(ptr[3], 0x00);

    CHECK_EQ(sph_buffer_get_u8(buf, 1), 0x00);
    CHECK_EQ(sph_buffer_get_u8(buf, 2), 0x50);
    sph_buffer_set_u8(buf, 1, 0xAA);
    CHECK_EQ(sph_buffer_get_u8(buf, 1), 0xAA);

    uint8_t extra[3] = { 0x00, 0x01, 0x02 };
    sph_buffer_append(buf, extra, 3);
    CHECK_EQ(sph_buffer_len(buf), 11);
    CHECK_EQ(sph_buffer_get_u8(buf, 8), 0x00);
    CHECK_EQ(sph_buffer_get_u8(buf, 9), 0x01);
    CHECK_EQ(sph_buffer_get_u8(buf, 10), 0x02);

    auto* sliced = sph_buffer_slice(buf, 7, 4);
    CHECK(sliced != nullptr);
    CHECK_EQ(sph_buffer_len(sliced), 4);
    CHECK_EQ(sph_buffer_get_u8(sliced, 0), 0x00);
    CHECK_EQ(sph_buffer_get_u8(sliced, 1), 0x00);
    CHECK_EQ(sph_buffer_get_u8(sliced, 2), 0x01);
    CHECK_EQ(sph_buffer_get_u8(sliced, 3), 0x02);

    sph_buffer_free(sliced);
    sph_buffer_free(buf);

    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/buffer.sph\";\n"
        "\n"
        "fn test_buffer(): i64 {\n"
        "    let b = buffer_alloc(1024);\n"
        "    buffer_set_u8(b, 0, 65 as u8);\n"
        "    buffer_set_u8(b, 1, 0 as u8);\n"
        "    buffer_set_u8(b, 2, 66 as u8);\n"
        "    let l: i64 = buffer_len(b);\n"
        "    buffer_free(b);\n"
        "    return l;\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_buf_sph.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));
}

TEST_CASE("Stdlib P1: Mutex and Thread synchronization primitives") {
    void* m = sph_mutex_create();
    CHECK(m != nullptr);
    sph_mutex_lock(m);
    sph_mutex_unlock(m);
    sph_mutex_destroy(m);

    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/thread.sph\";\n"
        "\n"
        "fn test_sync(): void {\n"
        "    let m = mutex_create();\n"
        "    mutex_lock(m);\n"
        "    thread_sleep(1);\n"
        "    mutex_unlock(m);\n"
        "    mutex_destroy(m);\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_thread_sph.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));
}

TEST_CASE("Stdlib P2: RFC 6455 WebSocket Client and Handshake") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/net.sph\";\n"
        "\n"
        "fn test_ws(): void {\n"
        "    let ws = ws_connect(\"127.0.0.1\", 4000, \"/ws\");\n"
        "    if (ws_is_open(ws)) {\n"
        "        ws_send_text(ws, \"hello server\");\n"
        "        ws_send_ping(ws);\n"
        "        let op: i32 = ws_recv(ws, 100);\n"
        "        if (op == 1) {\n"
        "            let msg: string = ws_get_message(ws);\n"
        "        }\n"
        "        ws_close(ws);\n"
        "    }\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_ws_sph.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));
}

TEST_CASE("Stdlib P3: Embedded HTTP Server and WebSocket Upgrade") {
    // 1. Verify C runtime server primitives
    int32_t s = sph_net_listen(29876);
    CHECK(s >= 0);
    // Poll non-blocking (0ms) - should report no connection without blocking
    CHECK_EQ(sph_server_poll_read(s, 0), 0);
    sph_net_close(s);

    // 2. Verify compilation of std/web.sph embedded server in SantientPHP
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/web.sph\";\n"
        "\n"
        "fn test_server(): void {\n"
        "    let s = server_listen(4000);\n"
        "    if (s >= 0) {\n"
        "        let client = server_accept(s, 10);\n"
        "        if (client >= 0) {\n"
        "            let req: string = server_read_request(client, 10);\n"
        "            server_write_response(client, 200, \"application/json\", \"{\\\"status\\\":\\\"ok\\\"}\");\n"
        "            server_close(client);\n"
        "        }\n"
        "        server_close(s);\n"
        "    }\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_server_sph.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));
}
