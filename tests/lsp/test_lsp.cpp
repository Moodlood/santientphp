#include "common/test_framework.hpp"
#include "compiler/lsp/Json.hpp"
#include "compiler/lsp/LspServer.hpp"
#include <sstream>

using namespace sph::lsp;

TEST_CASE("LSP: JsonValue parses and serializes data types") {
    // 1. Primitive types
    JsonValue nullVal;
    CHECK(nullVal.isNull());
    CHECK_EQ(nullVal.serialize(), "null");

    JsonValue boolVal(true);
    CHECK(boolVal.isBool());
    CHECK_EQ(boolVal.asBool(), true);
    CHECK_EQ(boolVal.serialize(), "true");

    JsonValue intVal(42);
    CHECK(intVal.isNumber());
    CHECK_EQ(intVal.asInt(), 42);
    CHECK_EQ(intVal.serialize(), "42");

    JsonValue strVal("hello \"world\"\n");
    CHECK(strVal.isString());
    CHECK_EQ(strVal.asString(), "hello \"world\"\n");
    CHECK_EQ(strVal.serialize(), "\"hello \\\"world\\\"\\n\"");

    // 2. Parsing
    std::string rawJson = "{\"name\": \"santient\", \"count\": 10, \"active\": true, \"tags\": [\"fast\", \"native\"]}";
    JsonValue parsed = JsonValue::parse(rawJson);
    CHECK(parsed.isObject());
    CHECK_EQ(parsed["name"].asString(), "santient");
    CHECK_EQ(parsed["count"].asInt(), 10);
    CHECK_EQ(parsed["active"].asBool(), true);
    CHECK(parsed["tags"].isArray());
    CHECK_EQ(parsed["tags"].asArray().size(), 2u);
    CHECK_EQ(parsed["tags"][0].asString(), "fast");
    CHECK_EQ(parsed["tags"][1].asString(), "native");
}

TEST_CASE("LSP: initialize, initialized, shutdown, and exit lifecycle") {
    LspServer server;

    // 1. initialize request
    std::string initReq = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
    std::string initResp = server.handleMessage(initReq);
    CHECK(!initResp.empty());

    JsonValue respObj = JsonValue::parse(initResp);
    CHECK_EQ(respObj["id"].asInt(), 1);
    CHECK(respObj["result"].isObject());
    CHECK(respObj["result"]["capabilities"].isObject());
    CHECK_EQ(respObj["result"]["capabilities"]["textDocumentSync"].asInt(), 1);
    CHECK_EQ(respObj["result"]["capabilities"]["hoverProvider"].asBool(), true);
    CHECK_EQ(respObj["result"]["capabilities"]["definitionProvider"].asBool(), true);
    CHECK_EQ(respObj["result"]["serverInfo"]["name"].asString(), "sphc-lsp");

    // 2. initialized notification
    std::string initializedNotif = "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}";
    std::string emptyResp = server.handleMessage(initializedNotif);
    CHECK(emptyResp.empty());
    CHECK(server.isInitialized());

    // 3. shutdown request
    std::string shutdownReq = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\",\"params\":{}}";
    std::string shutdownResp = server.handleMessage(shutdownReq);
    CHECK(!shutdownResp.empty());
    CHECK(server.isShutdown());

    // 4. exit notification
    std::string exitNotif = "{\"jsonrpc\":\"2.0\",\"method\":\"exit\",\"params\":{}}";
    server.handleMessage(exitNotif);
    CHECK(server.isExitRequested());
}

TEST_CASE("LSP: textDocument/didOpen and textDocument/didChange publish diagnostics") {
    LspServer server;
    std::ostringstream outputStream;

    // 1. Open document with syntax error (missing brace)
    std::string badCode =
        "<?php\n"
        "fn broken(): void {\n"
        "    let x = 10\n"; // missing semicolon and closing brace

    std::string didOpenBad =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test.sph\",\"languageId\":\"sph\",\"version\":1,"
        "\"text\":\"" + JsonValue(badCode).asString() + "\"}}}";

    server.handleMessage(didOpenBad, &outputStream);

    std::string outStr = outputStream.str();
    CHECK(!outStr.empty());
    CHECK(outStr.find("Content-Length:") != std::string::npos);
    CHECK(outStr.find("textDocument/publishDiagnostics") != std::string::npos);
    CHECK(outStr.find("test.sph") != std::string::npos);

    // 2. Fix document via didChange
    outputStream.str("");
    std::string goodCode =
        "<?php\n"
        "fn fixed(): i32 {\n"
        "    let x: i32 = 10;\n"
        "    return x;\n"
        "}\n";

    std::string didChangeGood =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test.sph\",\"version\":2},"
        "\"contentChanges\":[{\"text\":\"" + JsonValue(goodCode).asString() + "\"}]}}";

    server.handleMessage(didChangeGood, &outputStream);
    std::string fixStr = outputStream.str();
    CHECK(!fixStr.empty());
    CHECK(fixStr.find("publishDiagnostics") != std::string::npos);
    CHECK(fixStr.find("\"diagnostics\":[]") != std::string::npos);

    // 3. Close document
    outputStream.str("");
    std::string didClose =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didClose\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test.sph\"}}}";
    server.handleMessage(didClose, &outputStream);
    CHECK(server.getDocuments().find("file:///test.sph") == server.getDocuments().end());
}

TEST_CASE("LSP: textDocument/hover returns symbol signatures and doc markdown") {
    LspServer server;

    std::string code =
        "<?php\n"
        "fn calculate(factor: i32, offset: i32): i32 {\n"
        "    return factor + offset;\n"
        "}\n";

    std::string didOpen =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///math.sph\",\"languageId\":\"sph\",\"version\":1,"
        "\"text\":\"" + JsonValue(code).asString() + "\"}}}";

    server.handleMessage(didOpen);

    // Hover over 'fn' keyword (line 1, col 0)
    std::string hoverKwReq =
        "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"textDocument/hover\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///math.sph\"},"
        "\"position\":{\"line\":1,\"character\":1}}}";

    std::string hoverKwResp = server.handleMessage(hoverKwReq);
    CHECK(!hoverKwResp.empty());
    CHECK(hoverKwResp.find("Declares a function in SantientPHP") != std::string::npos);

    // Hover over function name 'calculate' (line 1, col 5)
    std::string hoverFnReq =
        "{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"textDocument/hover\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///math.sph\"},"
        "\"position\":{\"line\":1,\"character\":5}}}";

    std::string hoverFnResp = server.handleMessage(hoverFnReq);
    CHECK(!hoverFnResp.empty());
    CHECK(hoverFnResp.find("fn calculate(factor: i32, offset: i32): i32") != std::string::npos);
}

TEST_CASE("LSP: textDocument/completion returns keywords, types, and document symbols") {
    LspServer server;

    std::string code =
        "<?php\n"
        "fn helper(): void {}\n";

    std::string didOpen =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///complete.sph\",\"languageId\":\"sph\",\"version\":1,"
        "\"text\":\"" + JsonValue(code).asString() + "\"}}}";

    server.handleMessage(didOpen);

    std::string compReq =
        "{\"jsonrpc\":\"2.0\",\"id\":20,\"method\":\"textDocument/completion\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///complete.sph\"},"
        "\"position\":{\"line\":1,\"character\":0}}}";

    std::string compResp = server.handleMessage(compReq);
    CHECK(!compResp.empty());

    JsonValue compObj = JsonValue::parse(compResp);
    CHECK(compObj["result"].isArray());
    const auto& items = compObj["result"].asArray();
    CHECK(items.size() > 20u);

    bool foundFn = false;
    bool foundDynamic = false;
    bool foundHelper = false;
    bool foundPrintln = false;

    for (const auto& item : items) {
        std::string label = item["label"].asString();
        if (label == "fn") foundFn = true;
        if (label == "dynamic") foundDynamic = true;
        if (label == "helper") foundHelper = true;
        if (label == "println") foundPrintln = true;
    }

    CHECK(foundFn);
    CHECK(foundDynamic);
    CHECK(foundHelper);
    CHECK(foundPrintln);
}

TEST_CASE("LSP: textDocument/definition and textDocument/documentSymbol") {
    LspServer server;

    std::string code =
        "<?php\n"
        "struct Point {\n"
        "    x: f32,\n"
        "    y: f32\n"
        "}\n"
        "\n"
        "fn render(p: Point): void {}\n";

    std::string didOpen =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///symbols.sph\",\"languageId\":\"sph\",\"version\":1,"
        "\"text\":\"" + JsonValue(code).asString() + "\"}}}";

    server.handleMessage(didOpen);

    // 1. Definition of 'Point' at call/param site (line 6, col 16)
    std::string defReq =
        "{\"jsonrpc\":\"2.0\",\"id\":30,\"method\":\"textDocument/definition\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///symbols.sph\"},"
        "\"position\":{\"line\":6,\"character\":16}}}";

    std::string defResp = server.handleMessage(defReq);
    CHECK(!defResp.empty());
    JsonValue defObj = JsonValue::parse(defResp);
    CHECK_EQ(defObj["result"]["uri"].asString(), "file:///symbols.sph");
    CHECK_EQ(defObj["result"]["range"]["start"]["line"].asInt(), 1); // 0-indexed line 1 is line 2 (struct Point)

    // 2. Document Symbol outline
    std::string symReq =
        "{\"jsonrpc\":\"2.0\",\"id\":31,\"method\":\"textDocument/documentSymbol\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///symbols.sph\"}}}";

    std::string symResp = server.handleMessage(symReq);
    CHECK(!symResp.empty());
    JsonValue symObj = JsonValue::parse(symResp);
    CHECK(symObj["result"].isArray());
    const auto& syms = symObj["result"].asArray();
    CHECK_EQ(syms.size(), 2u);
    CHECK_EQ(syms[0]["name"].asString(), "render"); // functions first or order
    CHECK_EQ(syms[1]["name"].asString(), "Point");
}

