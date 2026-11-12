#include "common/test_framework.hpp"
#include "compiler/formatter/Formatter.hpp"
#include <filesystem>
#include <fstream>
#include <string>

using namespace sph;

TEST_CASE("Formatter: Normalizes operator spacing and commas") {
    Formatter fmt;
    std::string unformatted = "let x=10+20*3;\nfn foo(a:i32,b:i32):i32{\nreturn a+b;\n}\n";
    auto result = fmt.format(unformatted);

    CHECK(result.success);
    CHECK(result.changed);
    CHECK(result.formattedCode.find("let x = 10 + 20 * 3;") != std::string::npos);
    CHECK(result.formattedCode.find("fn foo(a: i32, b: i32): i32 {") != std::string::npos);
    CHECK(result.formattedCode.find("    return a + b;") != std::string::npos);
}

TEST_CASE("Formatter: Properly indents nested blocks and control flow") {
    Formatter fmt;
    std::string unformatted =
        "<?php\n"
        "fn calculate(val: i32): i32 {\n"
        "if (val > 0) {\n"
        "while (val < 10) {\n"
        "val = val + 1;\n"
        "}\n"
        "}\n"
        "return val;\n"
        "}\n";

    auto result = fmt.format(unformatted);
    CHECK(result.success);
    CHECK(result.formattedCode.find("    if (val > 0) {") != std::string::npos);
    CHECK(result.formattedCode.find("        while (val < 10) {") != std::string::npos);
    CHECK(result.formattedCode.find("            val = val + 1;") != std::string::npos);
    CHECK(result.formattedCode.find("        }") != std::string::npos);
    CHECK(result.formattedCode.find("    }") != std::string::npos);
}

TEST_CASE("Formatter: Preserves line and block comments with sentimental notes") {
    Formatter fmt;
    std::string unformatted =
        "<?php\n"
        "// Handcrafted with love at 2 AM\n"
        "fn dream(): string {\n"
        "    /* A gentle reminder to breathe */\n"
        "    let msg = \"PHP has wings\";\n"
        "    return msg;\n"
        "}\n";

    auto result = fmt.format(unformatted);
    CHECK(result.success);
    CHECK(result.formattedCode.find("// Handcrafted with love at 2 AM") != std::string::npos);
    CHECK(result.formattedCode.find("/* A gentle reminder to breathe */") != std::string::npos);
    CHECK(result.formattedCode.find("let msg = \"PHP has wings\";") != std::string::npos);
}

TEST_CASE("Formatter: Idempotency (formatting twice yields identical code)") {
    Formatter fmt;
    std::string unformatted =
        "<?php\n"
        "fn add(a: i32, b: i32): i32 {\n"
        "    return a + b;\n"
        "}\n"
        "fn main(): i32 {\n"
        "    let x: i32 = 100;\n"
        "    let y: i32 = 200;\n"
        "    return add(x, y);\n"
        "}\n";

    auto pass1 = fmt.format(unformatted);
    CHECK(pass1.success);

    auto pass2 = fmt.format(pass1.formattedCode);
    CHECK(pass2.success);
    CHECK_EQ(pass1.formattedCode, pass2.formattedCode);
    CHECK(!pass2.changed);
}

TEST_CASE("Formatter: File formatting and checking on disk") {
    Formatter fmt;
    std::filesystem::path tempFile = "test_temp_format.sph";

    std::ofstream out(tempFile);
    out << "fn add(a:i32,b:i32):i32{\nreturn a+b;\n}\n";
    out.close();

    CHECK(!fmt.isFileFormatted(tempFile));

    auto res = fmt.formatFile(tempFile, true);
    CHECK(res.success);
    CHECK(res.changed);

    CHECK(fmt.isFileFormatted(tempFile));

    std::error_code ec;
    std::filesystem::remove(tempFile, ec);
}

