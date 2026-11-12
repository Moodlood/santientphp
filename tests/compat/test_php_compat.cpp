#include "common/test_framework.hpp"
#include "compiler/codegen/Linker.hpp"
#include "compiler/codegen/llvm/LLVMEmitter.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

std::vector<std::string> runPhpCode(const std::string& code, const std::string& tag, bool& ok) {
    ok = false;
    sph::SourceManager sm;
    uint32_t fileId = sm.addSource(tag + ".sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program) return {};

    sph::SemanticAnalyzer analyzer(emitter);
    if (!analyzer.analyze(*program)) return {};

    sph::IRGenerator irGen;
    auto irModule = irGen.generate(*program, tag + "_module");
    if (!irModule) return {};

    static uint32_t runCounter = 0;
    std::string uniqueTag = tag + "_" + std::to_string(++runCounter);
    std::filesystem::path tempAsm = uniqueTag + "_temp.s";
    std::filesystem::path tempExe = uniqueTag + "_temp.exe";
    std::filesystem::path outputLog = uniqueTag + "_temp_out.txt";
    std::filesystem::path runtimeLib = "build/lib/libsph_runtime.a";
    if (!std::filesystem::exists(runtimeLib)) {
        if (std::filesystem::exists("lib/libsph_runtime.a")) {
            runtimeLib = "lib/libsph_runtime.a";
        } else if (std::filesystem::exists("../build/lib/libsph_runtime.a")) {
            runtimeLib = "../build/lib/libsph_runtime.a";
        }
    }

    {
        std::ofstream asmFile(tempAsm);
        sph::X86_64CodeGen codegen(asmFile);
        codegen.generate(*irModule);
    }
    if (!std::filesystem::exists(tempAsm)) return {};

    sph::Linker linker;
    std::string errorMsg;
    bool linkOk = linker.linkExecutable(tempAsm, tempExe, runtimeLib, errorMsg);
    std::error_code ec;
    std::filesystem::remove(tempAsm, ec);

    if (!linkOk || !std::filesystem::exists(tempExe)) return {};

    std::string runCmd = ".\\" + tempExe.string() + " > " + outputLog.string();
    int runRes = std::system(runCmd.c_str());

    std::vector<std::string> lines;
    if (runRes == 0) {
        ok = true;
        std::ifstream outFile(outputLog);
        std::string l;
        while (std::getline(outFile, l)) {
            if (!l.empty() && l.back() == '\r') l.pop_back();
            lines.push_back(l);
        }
        outFile.close();
    }

    std::filesystem::remove(tempExe, ec);
    std::filesystem::remove(outputLog, ec);
    return lines;
}

} // namespace

TEST_CASE("PHP Compat: Dynamic typing, loose equality, and type coercion (Milestone 10A)") {
    std::string code =
        "<?php\n"
        "function main(): i32 {\n"
        "    $a = \"100\";\n"
        "    $b = 50;\n"
        "    $sum = $a + $b;\n"
        "    echo \"Sum: \" . $sum . \"\\n\";\n"
        "    $diff = $a - 25;\n"
        "    echo \"Diff: \" . $diff . \"\\n\";\n"
        "    $mul = $b * 2;\n"
        "    echo \"Mul: \" . $mul . \"\\n\";\n"
        "    $eq1 = \"false\";\n"
        "    if ($a == 100) { $eq1 = \"true\"; }\n"
        "    echo \"Eq1: \" . $eq1 . \"\\n\";\n"
        "    $eq2 = \"false\";\n"
        "    if ($a != 200) { $eq2 = \"true\"; }\n"
        "    echo \"Eq2: \" . $eq2 . \"\\n\";\n"
        "    $concat = \"Count: \" . 42 . \" items, active: \" . true . \"\\n\";\n"
        "    echo $concat;\n"
        "    return 0;\n"
        "}\n";

    bool ok = false;
    auto lines = runPhpCode(code, "test_compat_10a", ok);
    CHECK(ok);
    CHECK(lines.size() >= 6);
    if (lines.size() >= 6) {
        CHECK_EQ(lines[0], "Sum: 150");
        CHECK_EQ(lines[1], "Diff: 75");
        CHECK_EQ(lines[2], "Mul: 100");
        CHECK_EQ(lines[3], "Eq1: true");
        CHECK_EQ(lines[4], "Eq2: true");
        CHECK_EQ(lines[5], "Count: 42 items, active: 1");
    }
}

TEST_CASE("PHP Compat: Auto-vivification and associative array semantics (Milestone 10B)") {
    std::string code =
        "<?php\n"
        "function main(): i32 {\n"
        "    $config = [];\n"
        "    $config[\"db\"] = [];\n"
        "    $config[\"db\"][\"host\"] = \"localhost\";\n"
        "    $config[\"db\"][\"port\"] = 3306;\n"
        "    $config[\"app\"] = [];\n"
        "    $config[\"app\"][\"debug\"] = true;\n"
        "    echo \"Host: \" . $config[\"db\"][\"host\"] . \"\\n\";\n"
        "    echo \"Port: \" . $config[\"db\"][\"port\"] . \"\\n\";\n"
        "    echo \"Debug: \" . $config[\"app\"][\"debug\"] . \"\\n\";\n"
        "    $hasHost = \"no\";\n"
        "    if (array_key_exists(\"host\", $config[\"db\"])) { $hasHost = \"yes\"; }\n"
        "    echo \"HasHost: \" . $hasHost . \"\\n\";\n"
        "    $hasMissing = \"no\";\n"
        "    if (array_key_exists(\"pass\", $config[\"db\"])) { $hasMissing = \"yes\"; }\n"
        "    echo \"HasMissing: \" . $hasMissing . \"\\n\";\n"
        "    $dbKeys = array_keys($config[\"db\"]);\n"
        "    echo \"DbKeyCount: \" . count($dbKeys) . \"\\n\";\n"
        "    return 0;\n"
        "}\n";

    bool ok = false;
    auto lines = runPhpCode(code, "test_compat_10b", ok);
    CHECK(ok);
    CHECK(lines.size() >= 6);
    if (lines.size() >= 6) {
        CHECK_EQ(lines[0], "Host: localhost");
        CHECK_EQ(lines[1], "Port: 3306");
        CHECK_EQ(lines[2], "Debug: 1");
        CHECK_EQ(lines[3], "HasHost: yes");
        CHECK_EQ(lines[4], "HasMissing: no");
        CHECK_EQ(lines[5], "DbKeyCount: 2");
    }
}

TEST_CASE("PHP Compat: Control flow loops and PHP stdlib builtins (Milestone 10C)") {
    std::string code =
        "<?php\n"
        "function main(): i32 {\n"
        "    $items = [\"alpha\", \"beta\", \"gamma\"];\n"
        "    array_push($items, \"delta\");\n"
        "    echo \"Count: \" . count($items) . \"\\n\";\n"
        "    $csv = implode(\",\", $items);\n"
        "    echo \"Implode: \" . $csv . \"\\n\";\n"
        "    $parts = explode(\",\", $csv);\n"
        "    echo \"Explode count: \" . count($parts) . \"\\n\";\n"
        "    echo \"Substr: \" . substr(\"SantientPHP\", 0, 8) . \"\\n\";\n"
        "    echo \"Strlen: \" . strlen(\"SantientPHP\") . \"\\n\";\n"
        "    echo \"Strpos: \" . strpos(\"SantientPHP\", \"PHP\") . \"\\n\";\n"
        "    foreach ($items as $item) {\n"
        "        if ($item == \"beta\") { continue; }\n"
        "        if ($item == \"delta\") { break; }\n"
        "        echo \"Item: \" . $item . \"\\n\";\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    bool ok = false;
    auto lines = runPhpCode(code, "test_compat_10c", ok);
    CHECK(ok);
    CHECK(lines.size() >= 8);
    if (lines.size() >= 8) {
        CHECK_EQ(lines[0], "Count: 4");
        CHECK_EQ(lines[1], "Implode: alpha,beta,gamma,delta");
        CHECK_EQ(lines[2], "Explode count: 4");
        CHECK_EQ(lines[3], "Substr: Santient");
        CHECK_EQ(lines[4], "Strlen: 11");
        CHECK_EQ(lines[5], "Strpos: 8");
        CHECK_EQ(lines[6], "Item: alpha");
        CHECK_EQ(lines[7], "Item: gamma");
    }
}

TEST_CASE("PHP Compat: OOP Classes, methods, properties, and introspections (Milestone 10D)") {
    std::string code =
        "<?php\n"
        "class Person {\n"
        "    public $name;\n"
        "    public $role;\n"
        "    public function __construct($name, $role) {\n"
        "        $this->name = $name;\n"
        "        $this->role = $role;\n"
        "    }\n"
        "    public function describe() {\n"
        "        return $this->name . \" is a \" . $this->role;\n"
        "    }\n"
        "}\n"
        "function main(): i32 {\n"
        "    $p = new Person(\"Alice\", \"Developer\");\n"
        "    echo $p->describe() . \"\\n\";\n"
        "    $isObj = \"no\";\n"
        "    if (is_object($p)) { $isObj = \"yes\"; }\n"
        "    echo \"IsObject: \" . $isObj . \"\\n\";\n"
        "    echo \"Class: \" . get_class($p) . \"\\n\";\n"
        "    $hasName = \"no\";\n"
        "    if (property_exists($p, \"name\")) { $hasName = \"yes\"; }\n"
        "    echo \"HasName: \" . $hasName . \"\\n\";\n"
        "    $hasAge = \"no\";\n"
        "    if (property_exists($p, \"age\")) { $hasAge = \"yes\"; }\n"
        "    echo \"HasAge: \" . $hasAge . \"\\n\";\n"
        "    return 0;\n"
        "}\n";

    bool ok = false;
    auto lines = runPhpCode(code, "test_compat_10d", ok);
    CHECK(ok);
    CHECK(lines.size() >= 5);
    if (lines.size() >= 5) {
        CHECK_EQ(lines[0], "Alice is a Developer");
        CHECK_EQ(lines[1], "IsObject: yes");
        CHECK_EQ(lines[2], "Class: Person");
        CHECK_EQ(lines[3], "HasName: yes");
        CHECK_EQ(lines[4], "HasAge: no");
    }
}

TEST_CASE("PHP Compat: Exceptions, try/catch/finally, and pass-by-reference (Milestone 10E)") {
    std::string code =
        "<?php\n"
        "class CustomException {\n"
        "    public $message;\n"
        "    public function __construct($msg) {\n"
        "        $this->message = $msg;\n"
        "    }\n"
        "    public function getMessage() {\n"
        "        return $this->message;\n"
        "    }\n"
        "}\n"
        "function increment(&$val, $amount) {\n"
        "    $val = $val + $amount;\n"
        "}\n"
        "function risky($flag) {\n"
        "    if ($flag < 0) {\n"
        "        throw new CustomException(\"Negative value not allowed\");\n"
        "    }\n"
        "    return $flag * 2;\n"
        "}\n"
        "function main(): i32 {\n"
        "    $num = 40;\n"
        "    increment($num, 2);\n"
        "    echo \"Incremented: \" . $num . \"\\n\";\n"
        "    try {\n"
        "        echo \"Risky: \" . risky(5) . \"\\n\";\n"
        "        risky(-1);\n"
        "    } catch (CustomException $e) {\n"
        "        echo \"Caught: \" . $e->getMessage() . \"\\n\";\n"
        "    } finally {\n"
        "        echo \"Finally executed\\n\";\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    bool ok = false;
    auto lines = runPhpCode(code, "test_compat_10e", ok);
    CHECK(ok);
    CHECK(lines.size() >= 4);
    if (lines.size() >= 4) {
        CHECK_EQ(lines[0], "Incremented: 42");
        CHECK_EQ(lines[1], "Risky: 10");
        CHECK_EQ(lines[2], "Caught: Negative value not allowed");
        CHECK_EQ(lines[3], "Finally executed");
    }
}

TEST_CASE("PHP Compat: Database and HTTP interoperability (Milestone 10F)") {
    std::string code =
        "<?php\n"
        "function main(): i32 {\n"
        "    $pdo = new PDO(\"sqlite::memory:\");\n"
        "    $pdo->exec(\"CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, qty INTEGER);\");\n"
        "    $stmt = $pdo->prepare(\"INSERT INTO items (name, qty) VALUES (?, ?);\");\n"
        "    $stmt->execute([\"Gadget\", 5]);\n"
        "    $stmt->execute([\"Widget\", 12]);\n"
        "    $query = $pdo->query(\"SELECT name, qty FROM items ORDER BY id ASC;\");\n"
        "    $rows = $query->fetchAll(PDO::FETCH_ASSOC);\n"
        "    foreach ($rows as $r) {\n"
        "        echo \"Item: \" . $r[\"name\"] . \" (Qty: \" . $r[\"qty\"] . \")\\n\";\n"
        "    }\n"
        "    $ch = curl_init(\"http://mock/api/ping\");\n"
        "    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);\n"
        "    $resp = curl_exec($ch);\n"
        "    $code = curl_getinfo($ch, CURLINFO_HTTP_CODE);\n"
        "    curl_close($ch);\n"
        "    echo \"cURL Code: \" . $code . \"\\n\";\n"
        "    echo \"cURL Resp: \" . $resp . \"\\n\";\n"
        "    return 0;\n"
        "}\n";

    bool ok = false;
    auto lines = runPhpCode(code, "test_compat_10f", ok);
    CHECK(ok);
    CHECK(lines.size() >= 4);
    if (lines.size() >= 4) {
        CHECK_EQ(lines[0], "Item: Gadget (Qty: 5)");
        CHECK_EQ(lines[1], "Item: Widget (Qty: 12)");
        CHECK_EQ(lines[2], "cURL Code: 200");
        CHECK_EQ(lines[3], "cURL Resp: {\"status\":\"ok\",\"message\":\"SantientPHP HTTP service active\",\"url\":\"http://mock/api/ping\"}");
    }
}

TEST_CASE("PHP Compat: End-to-end Store Application (Milestone 10G / examples/php_app.sph)") {
    std::filesystem::path appPath = "examples/php_app.sph";
    if (!std::filesystem::exists(appPath) && std::filesystem::exists("../examples/php_app.sph")) {
        appPath = "../examples/php_app.sph";
    }
    std::ifstream srcFile(appPath);
    CHECK(srcFile.is_open());
    std::stringstream buffer;
    buffer << srcFile.rdbuf();
    std::string appCode = buffer.str();
    CHECK(!appCode.empty());

    bool ok = false;
    auto lines = runPhpCode(appCode, "test_compat_10g_app", ok);
    CHECK(ok);
    CHECK(lines.size() >= 17);
    if (lines.size() >= 17) {
        CHECK_EQ(lines[0], "=== SantientPHP Store Application ===");
        CHECK_EQ(lines[1], "Product 1: Mechanical Keyboard ($100)");
        CHECK_EQ(lines[2], "Product 2: 4K Monitor ($300)");
        CHECK_EQ(lines[3], "Discounted Product 1: $90");
        CHECK_EQ(lines[4], "Discounted Product 2: $240");
        CHECK_EQ(lines[5], "Validating bad order...");
        CHECK_EQ(lines[6], "Caught Expected Exception: Customer name cannot be empty");
        CHECK_EQ(lines[7], "Validation check completed");
        CHECK_EQ(lines[8], "--- Recorded Orders ---");
        CHECK_EQ(lines[9], "Order: Alice -> Mechanical Keyboard ($90)");
        CHECK_EQ(lines[10], "Order: Bob -> 4K Monitor ($240)");
        CHECK_EQ(lines[11], "Total Orders: 2");
        CHECK_EQ(lines[12], "Total Revenue: $330");
        CHECK_EQ(lines[13], "--- HTTP Reporting ---");
        CHECK_EQ(lines[14], "Audit Status: 200");
        CHECK_EQ(lines[15], "Audit Response: {\"status\":\"ok\",\"action\":\"created\",\"received\":\"count=2&rev=330\"}");
        CHECK_EQ(lines[16], "Remote Config: {\"status\":\"ok\",\"message\":\"SantientPHP HTTP service active\",\"url\":\"http://mock/api/config\"}");
        CHECK_EQ(lines[17], "SantientPHP Phase 10 Application Verified!");
    }
}
