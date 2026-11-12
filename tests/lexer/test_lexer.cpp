#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/lexer/Lexer.hpp"
#include "compiler/source/SourceManager.hpp"

TEST_CASE("Lexer tokenizes opening tag, keywords and types") {
    sph::SourceManager sm;
    std::string code = "<?php\nfn let mut return if else while struct enum i32 i64 f32 f64 bool string void true false";
    uint32_t fileId = sm.addSource("test.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());

    std::vector<sph::TokenKind> expected = {
        sph::TokenKind::PhpOpenTag,
        sph::TokenKind::KwFn,
        sph::TokenKind::KwLet,
        sph::TokenKind::KwMut,
        sph::TokenKind::KwReturn,
        sph::TokenKind::KwIf,
        sph::TokenKind::KwElse,
        sph::TokenKind::KwWhile,
        sph::TokenKind::KwStruct,
        sph::TokenKind::KwEnum,
        sph::TokenKind::KwI32,
        sph::TokenKind::KwI64,
        sph::TokenKind::KwF32,
        sph::TokenKind::KwF64,
        sph::TokenKind::KwBool,
        sph::TokenKind::KwString,
        sph::TokenKind::KwVoid,
        sph::TokenKind::KwTrue,
        sph::TokenKind::KwFalse,
        sph::TokenKind::EndOfFile
    };

    CHECK_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < expected.size() && i < tokens.size(); ++i) {
        CHECK_EQ(static_cast<int>(tokens[i].kind), static_cast<int>(expected[i]));
    }
}

TEST_CASE("Lexer tokenizes numbers and strings") {
    sph::SourceManager sm;
    std::string code = "<?php\n100 0xFF 0b1010 3.14 1e-4 \"hello\\nworld\" 'simple'";
    uint32_t fileId = sm.addSource("literals.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[1].text, "100");

    CHECK_EQ(tokens[2].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[2].text, "0xFF");

    CHECK_EQ(tokens[3].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[3].text, "0b1010");

    CHECK_EQ(tokens[4].kind, sph::TokenKind::FloatLiteral);
    CHECK_EQ(tokens[4].text, "3.14");

    CHECK_EQ(tokens[5].kind, sph::TokenKind::FloatLiteral);
    CHECK_EQ(tokens[5].text, "1e-4");

    CHECK_EQ(tokens[6].kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(tokens[6].text, "hello\nworld");

    CHECK_EQ(tokens[7].kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(tokens[7].text, "simple");
}

TEST_CASE("Lexer tokenizes operators and punctuation") {
    sph::SourceManager sm;
    std::string code = "<?php\n+ - * / % = == != < <= > >= && || ! ; : , . ( ) { } [ ]";
    uint32_t fileId = sm.addSource("ops.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());

    std::vector<sph::TokenKind> expectedOps = {
        sph::TokenKind::PhpOpenTag,
        sph::TokenKind::Plus,
        sph::TokenKind::Minus,
        sph::TokenKind::Star,
        sph::TokenKind::Slash,
        sph::TokenKind::Percent,
        sph::TokenKind::Equal,
        sph::TokenKind::EqualEqual,
        sph::TokenKind::ExclaimEqual,
        sph::TokenKind::Less,
        sph::TokenKind::LessEqual,
        sph::TokenKind::Greater,
        sph::TokenKind::GreaterEqual,
        sph::TokenKind::AmpAmp,
        sph::TokenKind::PipePipe,
        sph::TokenKind::Exclaim,
        sph::TokenKind::Semicolon,
        sph::TokenKind::Colon,
        sph::TokenKind::Comma,
        sph::TokenKind::Dot,
        sph::TokenKind::LParen,
        sph::TokenKind::RParen,
        sph::TokenKind::LBrace,
        sph::TokenKind::RBrace,
        sph::TokenKind::LBracket,
        sph::TokenKind::RBracket,
        sph::TokenKind::EndOfFile
    };

    CHECK_EQ(tokens.size(), expectedOps.size());
    for (size_t i = 0; i < expectedOps.size() && i < tokens.size(); ++i) {
        CHECK_EQ(static_cast<int>(tokens[i].kind), static_cast<int>(expectedOps[i]));
    }
}

TEST_CASE("Lexer tokenizes comments correctly") {
    sph::SourceManager sm;
    std::string code = "<?php\n// single line comment\nlet x = 10; /* multi\nline\ncomment */ let y = 20;";
    uint32_t fileId = sm.addSource("comments.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());

    // Expect: <?php, let, x, =, 10, ;, let, y, =, 20, ;, EOF
    std::vector<sph::TokenKind> expected = {
        sph::TokenKind::PhpOpenTag,
        sph::TokenKind::KwLet,
        sph::TokenKind::Identifier,
        sph::TokenKind::Equal,
        sph::TokenKind::IntLiteral,
        sph::TokenKind::Semicolon,
        sph::TokenKind::KwLet,
        sph::TokenKind::Identifier,
        sph::TokenKind::Equal,
        sph::TokenKind::IntLiteral,
        sph::TokenKind::Semicolon,
        sph::TokenKind::EndOfFile
    };

    CHECK_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < expected.size() && i < tokens.size(); ++i) {
        CHECK_EQ(static_cast<int>(tokens[i].kind), static_cast<int>(expected[i]));
    }
}

TEST_CASE("Lexer tokenizes SPH 0.1 acceptance program") {
    sph::SourceManager sm;
    std::string code =
        "<?php\n"
        "\n"
        "fn add(a: i32, b: i32): i32 {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "fn main(): i32 {\n"
        "    let first: i32 = 100;\n"
        "    let second: i32 = 200;\n"
        "    let result = add(first, second);\n"
        "\n"
        "    println(result);\n"
        "    return 0;\n"
        "}\n";

    uint32_t fileId = sm.addSource("examples/hello.sph", code);
    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK(tokens.size() > 20);
    CHECK_EQ(tokens.front().kind, sph::TokenKind::PhpOpenTag);
    CHECK_EQ(tokens.back().kind, sph::TokenKind::EndOfFile);
}

TEST_CASE("Lexer detects unterminated string error") {
    sph::SourceManager sm;
    std::string code = "<?php\nlet s = \"unterminated;\n";
    uint32_t fileId = sm.addSource("err_str.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(emitter.hasErrors());
    CHECK_EQ(emitter.getErrorCount(), 1u);
}

TEST_CASE("Lexer tokenizes enum keyword and double colon") {
    sph::SourceManager sm;
    std::string code = "<?php\nenum Status::Active";
    uint32_t fileId = sm.addSource("enum_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwEnum);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[2].text, "Status");
    CHECK_EQ(tokens[3].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[4].text, "Active");
}

TEST_CASE("Lexer tokenizes import keyword") {
    sph::SourceManager sm;
    std::string code = "<?php\nimport \"math.sph\";";
    uint32_t fileId = sm.addSource("import_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwImport);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(tokens[2].text, "math.sph");
    CHECK_EQ(tokens[3].kind, sph::TokenKind::Semicolon);
}

TEST_CASE("Lexer tokenizes dot-dot and ampersand for slices") {
    sph::SourceManager sm;
    std::string code = "<?php\n&[i32] 1..4 ..5 2..";
    uint32_t fileId = sm.addSource("slice_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Amp);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::LBracket);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::KwI32);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::RBracket);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[5].text, "1");
    CHECK_EQ(tokens[6].kind, sph::TokenKind::DotDot);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[7].text, "4");
    CHECK_EQ(tokens[8].kind, sph::TokenKind::DotDot);
    CHECK_EQ(tokens[9].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[9].text, "5");
    CHECK_EQ(tokens[10].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[10].text, "2");
    CHECK_EQ(tokens[11].kind, sph::TokenKind::DotDot);
}

TEST_CASE("Lexer tokenizes class and this keywords") {
    sph::SourceManager sm;
    std::string code = "<?php\nclass BankAccount { this.balance }";
    uint32_t fileId = sm.addSource("class_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwClass);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[2].text, "BankAccount");
    CHECK_EQ(tokens[3].kind, sph::TokenKind::LBrace);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::KwThis);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::Dot);
    CHECK_EQ(tokens[6].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[6].text, "balance");
    CHECK_EQ(tokens[7].kind, sph::TokenKind::RBrace);
}

TEST_CASE("Lexer tokenizes interface and implements keywords") {
    sph::SourceManager sm;
    std::string code = "<?php\ninterface Shape {} class Rect implements Shape {}";
    uint32_t fileId = sm.addSource("interface_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwInterface);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[2].text, "Shape");
    CHECK_EQ(tokens[3].kind, sph::TokenKind::LBrace);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::RBrace);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::KwClass);
    CHECK_EQ(tokens[6].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[6].text, "Rect");
    CHECK_EQ(tokens[7].kind, sph::TokenKind::KwImplements);
    CHECK_EQ(tokens[8].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[8].text, "Shape");
    CHECK_EQ(tokens[9].kind, sph::TokenKind::LBrace);
    CHECK_EQ(tokens[10].kind, sph::TokenKind::RBrace);
}

TEST_CASE("Lexer tokenizes unsafe keyword and pointer operators") {
    sph::SourceManager sm;
    std::string code = "<?php\nunsafe { let p: *i32 = &x; *p = 99; }";
    uint32_t fileId = sm.addSource("unsafe_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwUnsafe);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::LBrace);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::KwLet);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::Colon);
    CHECK_EQ(tokens[6].kind, sph::TokenKind::Star);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::KwI32);
    CHECK_EQ(tokens[7].text, "i32");
    CHECK_EQ(tokens[8].kind, sph::TokenKind::Equal);
    CHECK_EQ(tokens[9].kind, sph::TokenKind::Amp);
    CHECK_EQ(tokens[10].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[10].text, "x");
    CHECK_EQ(tokens[11].kind, sph::TokenKind::Semicolon);
    CHECK_EQ(tokens[12].kind, sph::TokenKind::Star);
    CHECK_EQ(tokens[13].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[13].text, "p");
    CHECK_EQ(tokens[14].kind, sph::TokenKind::Equal);
    CHECK_EQ(tokens[15].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[15].text, "99");
    CHECK_EQ(tokens[16].kind, sph::TokenKind::Semicolon);
    CHECK_EQ(tokens[17].kind, sph::TokenKind::RBrace);
}

TEST_CASE("Lexer tokenizes extern keyword and string ABI") {
    sph::SourceManager sm;
    std::string code = "<?php\nextern \"C\" fn abs(n: i32): i32;";
    uint32_t fileId = sm.addSource("extern_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwExtern);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(tokens[2].text, "C");
    CHECK_EQ(tokens[3].kind, sph::TokenKind::KwFn);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[4].text, "abs");
    CHECK_EQ(tokens[5].kind, sph::TokenKind::LParen);
    CHECK_EQ(tokens[6].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[6].text, "n");
    CHECK_EQ(tokens[7].kind, sph::TokenKind::Colon);
    CHECK_EQ(tokens[8].kind, sph::TokenKind::KwI32);
    CHECK_EQ(tokens[9].kind, sph::TokenKind::RParen);
    CHECK_EQ(tokens[10].kind, sph::TokenKind::Colon);
    CHECK_EQ(tokens[11].kind, sph::TokenKind::KwI32);
    CHECK_EQ(tokens[12].kind, sph::TokenKind::Semicolon);
}

TEST_CASE("Lexer tokenizes bitwise operators, shift operators, and as keyword") {
    sph::SourceManager sm;
    std::string code = "<?php\na & b | c ^ ~d << 2 >> 1 as i64";
    uint32_t fileId = sm.addSource("bitwise_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Amp);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::Pipe);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[6].kind, sph::TokenKind::Caret);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::Tilde);
    CHECK_EQ(tokens[8].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[9].kind, sph::TokenKind::LessLess);
    CHECK_EQ(tokens[10].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[11].kind, sph::TokenKind::GreaterGreater);
    CHECK_EQ(tokens[12].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[13].kind, sph::TokenKind::KwAs);
    CHECK_EQ(tokens[14].kind, sph::TokenKind::KwI64);
}

TEST_CASE("Lexer tokenizes hash symbol and attribute syntax") {
    sph::SourceManager sm;
    std::string code = "<?php\n#[repr(packed)] #[align(16)] struct S { x: i32 }";
    uint32_t fileId = sm.addSource("attr_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Hash);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::LBracket);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[3].text, "repr");
    CHECK_EQ(tokens[4].kind, sph::TokenKind::LParen);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[5].text, "packed");
    CHECK_EQ(tokens[6].kind, sph::TokenKind::RParen);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::RBracket);
    CHECK_EQ(tokens[8].kind, sph::TokenKind::Hash);
}

TEST_CASE("Lexer tokenizes Rc keyword") {
    sph::SourceManager sm;
    std::string code = "<?php\nRc<Point> Rc::new Rc::count";
    uint32_t fileId = sm.addSource("rc_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwRc);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Less);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[3].text, "Point");
    CHECK_EQ(tokens[4].kind, sph::TokenKind::Greater);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::KwRc);
    CHECK_EQ(tokens[6].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::KwNew);
    CHECK_EQ(tokens[7].text, "new");
    CHECK_EQ(tokens[8].kind, sph::TokenKind::KwRc);
    CHECK_EQ(tokens[9].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[10].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[10].text, "count");
}

TEST_CASE("Lexer tokenizes Box keyword") {
    sph::SourceManager sm;
    std::string code = "<?php\nBox<Node> Box::new";
    uint32_t fileId = sm.addSource("box_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwBox);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Less);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[3].text, "Node");
    CHECK_EQ(tokens[4].kind, sph::TokenKind::Greater);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::KwBox);
    CHECK_EQ(tokens[6].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::KwNew);
    CHECK_EQ(tokens[7].text, "new");
}

TEST_CASE("Lexer tokenizes Arena keyword") {
    sph::SourceManager sm;
    std::string code = "<?php\nArena Arena::new Arena::alloc Arena::reset Arena::destroy";
    uint32_t fileId = sm.addSource("arena_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwArena);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::KwArena);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::KwNew);
    CHECK_EQ(tokens[4].text, "new");
    CHECK_EQ(tokens[5].kind, sph::TokenKind::KwArena);
    CHECK_EQ(tokens[6].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[7].text, "alloc");
    CHECK_EQ(tokens[8].kind, sph::TokenKind::KwArena);
    CHECK_EQ(tokens[9].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[10].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[10].text, "reset");
    CHECK_EQ(tokens[11].kind, sph::TokenKind::KwArena);
    CHECK_EQ(tokens[12].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[13].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[13].text, "destroy");
}

TEST_CASE("Lexer tokenizes Vec keyword") {
    sph::SourceManager sm;
    std::string code = "<?php\nVec<i32> Vec::new Vec::with_capacity";
    uint32_t fileId = sm.addSource("vec_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwVec);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Less);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::KwI32);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::Greater);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::KwVec);
    CHECK_EQ(tokens[6].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::KwNew);
    CHECK_EQ(tokens[7].text, "new");
    CHECK_EQ(tokens[8].kind, sph::TokenKind::KwVec);
    CHECK_EQ(tokens[9].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[10].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[10].text, "with_capacity");
}

TEST_CASE("Lexer tokenizes PHP variables and echo statement") {
    sph::SourceManager sm;
    std::string code = "<?php\necho $user . $count;";
    uint32_t fileId = sm.addSource("php_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwEcho);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[2].text, "$user");
    CHECK_EQ(tokens[3].kind, sph::TokenKind::Dot);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[4].text, "$count");
    CHECK_EQ(tokens[5].kind, sph::TokenKind::Semicolon);
}

TEST_CASE("Lexer tokenizes double arrow operator for associative arrays") {
    sph::SourceManager sm;
    std::string code = "<?php\n[\"key\" => $val];";
    uint32_t fileId = sm.addSource("fat_arrow.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::LBracket);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::DoubleArrow);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[4].text, "$val");
    CHECK_EQ(tokens[5].kind, sph::TokenKind::RBracket);
}

TEST_CASE("Lexer tokenizes foreach, break, and continue keywords") {
    sph::SourceManager sm;
    std::string code = "<?php\nforeach ($items as $k => $v) { break; continue; }";
    uint32_t fileId = sm.addSource("foreach_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwForeach);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::LParen);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[3].text, "$items");
    CHECK_EQ(tokens[4].kind, sph::TokenKind::KwAs);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[5].text, "$k");
    CHECK_EQ(tokens[6].kind, sph::TokenKind::DoubleArrow);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[7].text, "$v");
    CHECK_EQ(tokens[8].kind, sph::TokenKind::RParen);
    CHECK_EQ(tokens[9].kind, sph::TokenKind::LBrace);
    CHECK_EQ(tokens[10].kind, sph::TokenKind::KwBreak);
    CHECK_EQ(tokens[11].kind, sph::TokenKind::Semicolon);
    CHECK_EQ(tokens[12].kind, sph::TokenKind::KwContinue);
    CHECK_EQ(tokens[13].kind, sph::TokenKind::Semicolon);
    CHECK_EQ(tokens[14].kind, sph::TokenKind::RBrace);
}

TEST_CASE("Lexer tokenizes new, function, visibility keywords and arrow operator") {
    sph::SourceManager sm;
    std::string code = "<?php\npublic function test() { $obj = new Person(); $obj->run(); }";
    uint32_t fileId = sm.addSource("php_oop_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwPublic);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::KwFunction);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[3].text, "test");
    CHECK_EQ(tokens[4].kind, sph::TokenKind::LParen);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::RParen);
    CHECK_EQ(tokens[6].kind, sph::TokenKind::LBrace);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[7].text, "$obj");
    CHECK_EQ(tokens[8].kind, sph::TokenKind::Equal);
    CHECK_EQ(tokens[9].kind, sph::TokenKind::KwNew);
    CHECK_EQ(tokens[10].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[10].text, "Person");
    CHECK_EQ(tokens[11].kind, sph::TokenKind::LParen);
    CHECK_EQ(tokens[12].kind, sph::TokenKind::RParen);
    CHECK_EQ(tokens[13].kind, sph::TokenKind::Semicolon);
    CHECK_EQ(tokens[14].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[14].text, "$obj");
    CHECK_EQ(tokens[15].kind, sph::TokenKind::Arrow);
    CHECK_EQ(tokens[16].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[16].text, "run");
}

TEST_CASE("Lexer tokenizes try, catch, finally, throw, and ampersand reference") {
    sph::SourceManager sm;
    std::string code = "<?php\ntry { throw new Exception(\"oops\"); } catch (Exception $e) { } finally { } function test(&$val) {}";
    uint32_t fileId = sm.addSource("exceptions_lex.sph", code);

    sph::DiagnosticEmitter emitter(sm);
    sph::Lexer lexer(fileId, sm, &emitter);
    auto tokens = lexer.tokenizeAll();

    CHECK(!emitter.hasErrors());
    CHECK_EQ(tokens[0].kind, sph::TokenKind::PhpOpenTag);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::KwTry);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::LBrace);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::KwThrow);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::KwNew);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[5].text, "Exception");
    CHECK_EQ(tokens[6].kind, sph::TokenKind::LParen);
    CHECK_EQ(tokens[7].kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(tokens[8].kind, sph::TokenKind::RParen);
    CHECK_EQ(tokens[9].kind, sph::TokenKind::Semicolon);
    CHECK_EQ(tokens[10].kind, sph::TokenKind::RBrace);
    CHECK_EQ(tokens[11].kind, sph::TokenKind::KwCatch);
    CHECK_EQ(tokens[12].kind, sph::TokenKind::LParen);
    CHECK_EQ(tokens[13].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[13].text, "Exception");
    CHECK_EQ(tokens[14].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[14].text, "$e");
    CHECK_EQ(tokens[15].kind, sph::TokenKind::RParen);
    CHECK_EQ(tokens[16].kind, sph::TokenKind::LBrace);
    CHECK_EQ(tokens[17].kind, sph::TokenKind::RBrace);
    CHECK_EQ(tokens[18].kind, sph::TokenKind::KwFinally);
    CHECK_EQ(tokens[19].kind, sph::TokenKind::LBrace);
    CHECK_EQ(tokens[20].kind, sph::TokenKind::RBrace);
    CHECK_EQ(tokens[21].kind, sph::TokenKind::KwFunction);
    CHECK_EQ(tokens[22].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[22].text, "test");
    CHECK_EQ(tokens[23].kind, sph::TokenKind::LParen);
    CHECK_EQ(tokens[24].kind, sph::TokenKind::Amp);
    CHECK_EQ(tokens[25].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[25].text, "$val");
    CHECK_EQ(tokens[26].kind, sph::TokenKind::RParen);
    CHECK_EQ(tokens[27].kind, sph::TokenKind::LBrace);
    CHECK_EQ(tokens[28].kind, sph::TokenKind::RBrace);
    CHECK_EQ(tokens[29].kind, sph::TokenKind::EndOfFile);
}




