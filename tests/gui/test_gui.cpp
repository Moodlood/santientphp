#include "common/test_framework.hpp"
#include "compiler/codegen/Linker.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/doctor/Doctor.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/lsp/LspServer.hpp"
#include "compiler/manifest/Manifest.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include "runtime/gui/gui.h"

#include <filesystem>
#include <sstream>

// ============================================================================
// 1. Manifest Subsystem and Icon Parsing
// ============================================================================

TEST_CASE("GUI: Manifest parses subsystem = 'windows' and icon path") {
    std::string toml =
        "[project]\n"
        "name = \"gui_demo\"\n"
        "version = \"1.0.0\"\n"
        "entry = \"src/main.sph\"\n"
        "\n"
        "[build]\n"
        "mode = \"release\"\n"
        "optimization = \"2\"\n"
        "subsystem = \"windows\"\n"
        "icon = \"assets/app.ico\"\n";

    auto manifestOpt = sph::Manifest::fromString(toml);
    CHECK(manifestOpt.has_value());
    if (manifestOpt) {
        CHECK_EQ(manifestOpt->projectName, "gui_demo");
        CHECK_EQ(manifestOpt->subsystem, "windows");
        CHECK_EQ(manifestOpt->icon, "assets/app.ico");
        CHECK(manifestOpt->isGuiSubsystem());
    }
}

TEST_CASE("GUI: Manifest defaults to console subsystem") {
    std::string toml =
        "[project]\n"
        "name = \"cli_tool\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[build]\n"
        "subsystem = \"console\"\n";

    auto manifestOpt = sph::Manifest::fromString(toml);
    CHECK(manifestOpt.has_value());
    if (manifestOpt) {
        CHECK_EQ(manifestOpt->subsystem, "console");
        CHECK(!manifestOpt->isGuiSubsystem());
    }
}

// ============================================================================
// 2. SIMD Framebuffer Canvas Surface and Pixel Blitting
// ============================================================================

TEST_CASE("GUI: Canvas2D creates pixel buffer, clears, and fills rectangles") {
    int width = 64;
    int height = 64;
    sph_canvas* canvas = sph_canvas_create(NULL, width, height);
    CHECK(canvas != nullptr);
    CHECK_EQ(canvas->width, width);
    CHECK_EQ(canvas->height, height);

    uint32_t* pixels = sph_canvas_get_pixels(canvas);
    CHECK(pixels != nullptr);

    // Clear with opaque blue: 0xFF0000FF (ARGB/RGBA)
    uint32_t blue = 0xFF0000FF;
    sph_canvas_clear(canvas, blue);
    CHECK_EQ(pixels[0], blue);
    CHECK_EQ(pixels[width * height / 2], blue);
    CHECK_EQ(pixels[width * height - 1], blue);

    // Fill rect with opaque red: 0xFFFF0000 at (10, 10, 20, 20)
    uint32_t red = 0xFFFF0000;
    sph_canvas_fill_rect(canvas, 10, 10, 20, 20, red);

    // Coordinate (15, 15) must be red
    CHECK_EQ(pixels[15 * width + 15], red);
    // Coordinate (5, 5) remains blue
    CHECK_EQ(pixels[5 * width + 5], blue);
    // Coordinate (35, 35) remains blue
    CHECK_EQ(pixels[35 * width + 35], blue);

    // Draw hollow rectangle with green: 0xFF00FF00
    uint32_t green = 0xFF00FF00;
    sph_canvas_draw_rect(canvas, 0, 0, 10, 10, green);
    CHECK_EQ(pixels[0], green);
    CHECK_EQ(pixels[9], green);
    CHECK_EQ(pixels[9 * width], green);

    sph_canvas_destroy(canvas);
}

TEST_CASE("GUI: Canvas2D SIMD alpha blending") {
    int width = 32;
    int height = 32;
    sph_canvas* canvas = sph_canvas_create(NULL, width, height);
    CHECK(canvas != nullptr);

    // Clear canvas to black: 0xFF000000
    sph_canvas_clear(canvas, 0xFF000000);

    // Create a 4x4 white source sprite: 0xFFFFFFFF
    uint32_t sprite[16];
    for (int i = 0; i < 16; ++i) {
        sprite[i] = 0xFFFFFFFF;
    }

    // Blend sprite at (4, 4) with 50% alpha (0.5f)
    sph_canvas_blend_simd(canvas, sprite, 4, 4, 4, 4, 0.5f);

    uint32_t* pixels = sph_canvas_get_pixels(canvas);
    uint32_t blended = pixels[4 * width + 4];

    // Blended pixel should have components around ~127 (0x7F)
    uint32_t r = (blended >> 16) & 0xFF;
    uint32_t g = (blended >> 8) & 0xFF;
    uint32_t b = blended & 0xFF;

    CHECK(r >= 120 && r <= 135);
    CHECK(g >= 120 && g <= 135);
    CHECK(b >= 120 && b <= 135);

    sph_canvas_destroy(canvas);
}

// ============================================================================
// 3. Thread-Safe UI Dispatch Queue
// ============================================================================

static int g_test_task_counter = 0;

static void test_task_add(void* arg) {
    intptr_t delta = (intptr_t)arg;
    g_test_task_counter += (int)delta;
}

TEST_CASE("GUI: Thread-safe post_to_main task dispatch queue") {
    g_test_task_counter = 0;

    sph_gui_post_to_main(test_task_add, (void*)(intptr_t)10);
    sph_gui_post_to_main(test_task_add, (void*)(intptr_t)25);
    sph_gui_post_to_main(test_task_add, (void*)(intptr_t)5);

    // Counter before dispatch
    CHECK_EQ(g_test_task_counter, 0);

    // Dispatch pending tasks
    sph_gui_dispatch_posted_tasks();

    // 10 + 25 + 5 = 40
    CHECK_EQ(g_test_task_counter, 40);

    // Second dispatch should have no-op
    sph_gui_dispatch_posted_tasks();
    CHECK_EQ(g_test_task_counter, 40);
}

// ============================================================================
// 4. Standard Library std/gui.sph AST and Semantic Analysis
// ============================================================================

TEST_CASE("GUI: Stdlib import std/gui.sph parses and passes semantic analysis") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/gui.sph\";\n"
        "\n"
        "fn main(): i32 {\n"
        "    let win: Window = window_create(\"Test Window\", 800, 600);\n"
        "    let btn: Button = button_create(win, \"Click Me\", 20, 20, 100, 30, 101);\n"
        "    button_set_text(btn, \"Updated\");\n"
        "    let txt: string = button_get_text(btn);\n"
        "    let pb: ProgressBar = progressbar_create(win, 20, 70, 200, 25, 102);\n"
        "    progressbar_set_pos(pb, 50);\n"
        "    let cv: Canvas2D = canvas_create(win, 320, 240);\n"
        "    canvas_clear(cv, 0xFF1E1E1E);\n"
        "    canvas_fill_rect(cv, 10, 10, 100, 50, 0xFF00AAFF);\n"
        "    canvas_destroy(cv);\n"
        "    window_close(win);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_gui_app.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);

    sph::SemanticAnalyzer analyzer(emitter);
    CHECK(analyzer.analyze(*program));

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, "test_gui_module");
    CHECK(irModule != nullptr);
}

TEST_CASE("GUI: Stdlib PHP Dynamic Mode GuiWindow, GuiButton, and GuiInput") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "import \"std/gui.sph\";\n"
        "\n"
        "fn main(): i32 {\n"
        "    let win = new GuiWindow(\"Dynamic SPH App\", 640, 480);\n"
        "    let btn = new GuiButton(win, \"Submit\", 10, 10, 80, 30, 201);\n"
        "    let input = new GuiInput(win, \"Initial text\", 10, 50, 150, 30, 202);\n"
        "    win->show();\n"
        "    btn->setText(\"OK\");\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("test_dynamic_gui.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    CHECK(!emitter.hasErrors());
    CHECK(program != nullptr);
}

// ============================================================================
// 5. Toolchain Doctor GUI Subsystem Detection
// ============================================================================

TEST_CASE("GUI: Doctor checks Windows GUI Subsystems and GUI runtime library") {
    sph::Doctor doctor;
    auto checks = doctor.runChecks();
    CHECK(!checks.empty());

    bool foundGuiSubsystems = false;
    bool foundGuiRuntime = false;

    for (const auto& c : checks) {
        if (c.name == "Windows GUI Subsystems") {
            foundGuiSubsystems = true;
#if defined(_WIN32)
            CHECK(c.isHealthy);
#endif
        }
        if (c.name == "SantientPHP GUI Runtime") {
            foundGuiRuntime = true;
            CHECK(c.isHealthy);
        }
    }

#if defined(_WIN32)
    CHECK(foundGuiSubsystems);
#endif
    CHECK(foundGuiRuntime);
}

// ============================================================================
// 6. LSP Hover and Autocompletion for GUI Symbols
// ============================================================================

TEST_CASE("GUI: LSP completion returns Window, Canvas2D, and GuiWindow") {
    sph::lsp::LspServer server;

    server.handleMessage("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");

    std::string compReq =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/completion\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test.sph\"},"
        "\"position\":{\"line\":0,\"character\":0}}}";

    std::string compResp = server.handleMessage(compReq);
    CHECK(!compResp.empty());

    auto respObj = sph::lsp::JsonValue::parse(compResp);
    CHECK(respObj["result"].isArray());
    const auto& items = respObj["result"].asArray();

    bool foundWindow = false;
    bool foundCanvas = false;
    bool foundGuiWindow = false;
    bool foundPump = false;

    for (const auto& item : items) {
        std::string label = item["label"].asString();
        if (label == "Window") foundWindow = true;
        if (label == "Canvas2D") foundCanvas = true;
        if (label == "GuiWindow") foundGuiWindow = true;
        if (label == "sph_gui_pump_events") foundPump = true;
    }

    CHECK(foundWindow);
    CHECK(foundCanvas);
    CHECK(foundGuiWindow);
    CHECK(foundPump);
}

TEST_CASE("GUI: LSP hover returns markdown documentation for Canvas2D and Window") {
    sph::lsp::LspServer server;

    server.handleMessage("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");

    // Open virtual document with Canvas2D
    std::string docMsg =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///gui_doc.sph\",\"languageId\":\"santientphp\",\"version\":1,"
        "\"text\":\"Canvas2D Window\"}}}";
    server.handleMessage(docMsg);

    std::string hoverReq =
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/hover\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///gui_doc.sph\"},"
        "\"position\":{\"line\":0,\"character\":2}}}";

    std::string hoverResp = server.handleMessage(hoverReq);
    CHECK(!hoverResp.empty());

    auto respObj = sph::lsp::JsonValue::parse(hoverResp);
    CHECK(respObj["result"].isObject());
    std::string val = respObj["result"]["contents"]["value"].asString();
    CHECK(val.find("Canvas2D") != std::string::npos);
    CHECK(val.find("128-bit SIMD") != std::string::npos);
}

