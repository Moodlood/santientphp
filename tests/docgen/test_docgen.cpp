#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/docgen/DocComment.hpp"
#include "compiler/docgen/DocExtractor.hpp"
#include "compiler/docgen/DocGenerator.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/source/SourceManager.hpp"
#include <filesystem>
#include <fstream>
#include <string>

using namespace sph;

TEST_CASE("Docgen: DocCommentParser parses docblocks and tags") {
    std::string raw =
        "/**\n"
        " * Computes the Euclidean distance.\n"
        " * Takes coordinates and calculates the hypotenuse.\n"
        " * @param x Horizontal offset\n"
        " * @param y Vertical offset\n"
        " * @return Distance in floating point\n"
        " * @throws MathError If inputs are NaN\n"
        " */";

    DocComment doc = DocCommentParser::parse(raw);
    CHECK_EQ(doc.brief, "Computes the Euclidean distance.");
    CHECK(doc.detailed.find("Takes coordinates and calculates the hypotenuse.") != std::string::npos);

    CHECK_EQ(doc.tags.size(), size_t(4));
    const auto* pX = doc.findTag("param");
    CHECK(pX != nullptr);
    if (pX) {
        CHECK_EQ(pX->target, "x");
        CHECK_EQ(pX->description, "Horizontal offset");
    }

    const auto* ret = doc.findTag("return");
    CHECK(ret != nullptr);
    if (ret) {
        CHECK_EQ(ret->description, "Distance in floating point");
    }

    const auto* thr = doc.findTag("throws");
    CHECK(thr != nullptr);
    if (thr) {
        CHECK_EQ(thr->target, "MathError");
        CHECK_EQ(thr->description, "If inputs are NaN");
    }
}

TEST_CASE("Docgen: DocCommentParser parses triple slash comments") {
    std::string raw =
        "/// Calculates the square of a number.\n"
        "/// @param n Value to multiply\n"
        "/// @return Squared value\n";

    DocComment doc = DocCommentParser::parse(raw);
    CHECK_EQ(doc.brief, "Calculates the square of a number.");
    CHECK_EQ(doc.tags.size(), size_t(2));
}

TEST_CASE("Docgen: DocExtractor extracts documentation from AST") {
    SourceManager sm;
    std::string code =
        "<?php\n"
        "/**\n"
        " * Adds two integers together.\n"
        " * @param a First value\n"
        " * @param b Second value\n"
        " * @return Sum of a and b\n"
        " */\n"
        "fn add(a: i32, b: i32): i32 {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "/**\n"
        " * Represents a 2D vector coordinate.\n"
        " */\n"
        "struct Point2D {\n"
        "    x: f32;\n"
        "    y: f32;\n"
        "}\n";

    uint32_t fileId = sm.addSource("math_utils.sph", code);
    DiagnosticEmitter emitter(sm);
    Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    CHECK(program != nullptr);
    CHECK(!emitter.hasErrors());

    DocExtractor extractor(sm);
    DocModule module = extractor.extractModule(fileId, *program);

    CHECK_EQ(module.name, "math_utils");
    CHECK_EQ(module.functions.size(), size_t(1));
    CHECK_EQ(module.structs.size(), size_t(1));

    const auto& fn = module.functions[0];
    CHECK_EQ(fn.name, "add");
    CHECK_EQ(fn.returnType, "i32");
    CHECK_EQ(fn.doc.brief, "Adds two integers together.");
    CHECK_EQ(fn.params.size(), size_t(2));
    CHECK_EQ(fn.params[0].name, "a");
    CHECK_EQ(fn.params[0].description, "First value");
    CHECK_EQ(fn.params[1].name, "b");
    CHECK_EQ(fn.params[1].description, "Second value");

    const auto& st = module.structs[0];
    CHECK_EQ(st.name, "Point2D");
    CHECK_EQ(st.doc.brief, "Represents a 2D vector coordinate.");
    CHECK_EQ(st.fields.size(), size_t(2));
}

TEST_CASE("Docgen: DocGenerator produces Markdown and HTML output") {
    DocProject project;
    project.title = "MathLib API";
    project.version = "1.0.0";

    DocModule mod;
    mod.name = "geometry";

    DocFunction fn;
    fn.name = "area";
    fn.returnType = "f64";
    fn.doc.brief = "Computes the surface area.";
    DocParamInfo p;
    p.name = "radius";
    p.type = "f64";
    p.description = "Radius of circle";
    fn.params.push_back(p);
    mod.functions.push_back(fn);

    project.modules.push_back(mod);

    DocGenerator gen;

    // 1. Markdown
    std::string md = gen.generateMarkdown(project);
    CHECK(md.find("# MathLib API (v1.0.0)") != std::string::npos);
    CHECK(md.find("## Functions") != std::string::npos);
    CHECK(md.find("`area`") != std::string::npos);
    CHECK(md.find("| `radius` | `f64` | Radius of circle |") != std::string::npos);

    // 2. HTML
    std::string html = gen.generateHtml(project);
    CHECK(html.find("<!DOCTYPE html>") != std::string::npos);
    CHECK(html.find("<title>MathLib API</title>") != std::string::npos);
    CHECK(html.find("<nav id=\"sidebar\">") != std::string::npos);
    CHECK(html.find("<span class=\"badge badge-fn\">fn</span>") != std::string::npos);
    CHECK(html.find("function filterSymbols()") != std::string::npos);

    // 3. Write to file
    std::filesystem::path tempHtml = "test_doc_output.html";
    CHECK(gen.generateToFile(project, tempHtml, "html"));
    CHECK(std::filesystem::exists(tempHtml));

    std::error_code ec;
    std::filesystem::remove(tempHtml, ec);
}

