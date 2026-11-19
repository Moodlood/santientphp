#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/lexer/Lexer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <string>
#include <vector>

namespace {

std::vector<sph::Token> tokenize(const std::string& source) {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    uint32_t fileId = sm.addSource("test.sph", source);
    sph::Lexer lexer(fileId, sm, &emitter);
    return lexer.tokenizeAll();
}

sph::Token singleToken(const std::string& source) {
    auto tokens = tokenize(source);
    for (const auto& t : tokens) {
        if (t.kind != sph::TokenKind::PhpOpenTag && t.kind != sph::TokenKind::EndOfFile) {
            return t;
        }
    }
    return tokens.empty() ? sph::Token{} : tokens.back();
}

} // namespace

// =============================================================================
// Exhaustive Keyword Tokenization Tests (1 to 40)
// =============================================================================

TEST_CASE("LexerExhaustive: 001 Keyword fn") {
    auto t = singleToken("fn");
    CHECK_EQ(t.kind, sph::TokenKind::KwFn);
}

TEST_CASE("LexerExhaustive: 002 Keyword function") {
    auto t = singleToken("function");
    CHECK_EQ(t.kind, sph::TokenKind::KwFunction);
}

TEST_CASE("LexerExhaustive: 003 Keyword let") {
    auto t = singleToken("let");
    CHECK_EQ(t.kind, sph::TokenKind::KwLet);
}

TEST_CASE("LexerExhaustive: 004 Keyword mut") {
    auto t = singleToken("mut");
    CHECK_EQ(t.kind, sph::TokenKind::KwMut);
}

TEST_CASE("LexerExhaustive: 005 Keyword return") {
    auto t = singleToken("return");
    CHECK_EQ(t.kind, sph::TokenKind::KwReturn);
}

TEST_CASE("LexerExhaustive: 006 Keyword if") {
    auto t = singleToken("if");
    CHECK_EQ(t.kind, sph::TokenKind::KwIf);
}

TEST_CASE("LexerExhaustive: 007 Keyword else") {
    auto t = singleToken("else");
    CHECK_EQ(t.kind, sph::TokenKind::KwElse);
}

TEST_CASE("LexerExhaustive: 008 Keyword while") {
    auto t = singleToken("while");
    CHECK_EQ(t.kind, sph::TokenKind::KwWhile);
}

TEST_CASE("LexerExhaustive: 009 Keyword await") {
    auto t = singleToken("await");
    CHECK_EQ(t.kind, sph::TokenKind::KwAwait);
}

TEST_CASE("LexerExhaustive: 010 Keyword foreach") {
    auto t = singleToken("foreach");
    CHECK_EQ(t.kind, sph::TokenKind::KwForeach);
}

TEST_CASE("LexerExhaustive: 011 Keyword break") {
    auto t = singleToken("break");
    CHECK_EQ(t.kind, sph::TokenKind::KwBreak);
}

TEST_CASE("LexerExhaustive: 012 Keyword continue") {
    auto t = singleToken("continue");
    CHECK_EQ(t.kind, sph::TokenKind::KwContinue);
}

TEST_CASE("LexerExhaustive: 013 Keyword match") {
    auto t = singleToken("match");
    CHECK_EQ(t.kind, sph::TokenKind::KwMatch);
}

TEST_CASE("LexerExhaustive: 014 Keyword struct") {
    auto t = singleToken("struct");
    CHECK_EQ(t.kind, sph::TokenKind::KwStruct);
}

TEST_CASE("LexerExhaustive: 015 Keyword class") {
    auto t = singleToken("class");
    CHECK_EQ(t.kind, sph::TokenKind::KwClass);
}

TEST_CASE("LexerExhaustive: 016 Keyword interface") {
    auto t = singleToken("interface");
    CHECK_EQ(t.kind, sph::TokenKind::KwInterface);
}

TEST_CASE("LexerExhaustive: 017 Keyword implements") {
    auto t = singleToken("implements");
    CHECK_EQ(t.kind, sph::TokenKind::KwImplements);
}

TEST_CASE("LexerExhaustive: 018 Keyword enum") {
    auto t = singleToken("enum");
    CHECK_EQ(t.kind, sph::TokenKind::KwEnum);
}

TEST_CASE("LexerExhaustive: 019 Keyword unsafe") {
    auto t = singleToken("unsafe");
    CHECK_EQ(t.kind, sph::TokenKind::KwUnsafe);
}

TEST_CASE("LexerExhaustive: 020 Keyword defer") {
    auto t = singleToken("defer");
    CHECK_EQ(t.kind, sph::TokenKind::KwDefer);
}

TEST_CASE("LexerExhaustive: 021 Keyword extern") {
    auto t = singleToken("extern");
    CHECK_EQ(t.kind, sph::TokenKind::KwExtern);
}

TEST_CASE("LexerExhaustive: 022 Keyword as") {
    auto t = singleToken("as");
    CHECK_EQ(t.kind, sph::TokenKind::KwAs);
}

TEST_CASE("LexerExhaustive: 023 Keyword new") {
    auto t = singleToken("new");
    CHECK_EQ(t.kind, sph::TokenKind::KwNew);
}

TEST_CASE("LexerExhaustive: 024 Keyword try") {
    auto t = singleToken("try");
    CHECK_EQ(t.kind, sph::TokenKind::KwTry);
}

TEST_CASE("LexerExhaustive: 025 Keyword catch") {
    auto t = singleToken("catch");
    CHECK_EQ(t.kind, sph::TokenKind::KwCatch);
}

TEST_CASE("LexerExhaustive: 026 Keyword finally") {
    auto t = singleToken("finally");
    CHECK_EQ(t.kind, sph::TokenKind::KwFinally);
}

TEST_CASE("LexerExhaustive: 027 Keyword throw") {
    auto t = singleToken("throw");
    CHECK_EQ(t.kind, sph::TokenKind::KwThrow);
}

TEST_CASE("LexerExhaustive: 028 Keyword Rc") {
    auto t = singleToken("Rc");
    CHECK_EQ(t.kind, sph::TokenKind::KwRc);
}

TEST_CASE("LexerExhaustive: 029 Keyword Box") {
    auto t = singleToken("Box");
    CHECK_EQ(t.kind, sph::TokenKind::KwBox);
}

TEST_CASE("LexerExhaustive: 030 Keyword Arena") {
    auto t = singleToken("Arena");
    CHECK_EQ(t.kind, sph::TokenKind::KwArena);
}

TEST_CASE("LexerExhaustive: 031 Keyword Vec") {
    auto t = singleToken("Vec");
    CHECK_EQ(t.kind, sph::TokenKind::KwVec);
}

TEST_CASE("LexerExhaustive: 032 Keyword Option") {
    auto t = singleToken("Option");
    CHECK_EQ(t.kind, sph::TokenKind::KwOption);
}

TEST_CASE("LexerExhaustive: 033 Keyword Result") {
    auto t = singleToken("Result");
    CHECK_EQ(t.kind, sph::TokenKind::KwResult);
}

TEST_CASE("LexerExhaustive: 034 Keyword public") {
    auto t = singleToken("public");
    CHECK_EQ(t.kind, sph::TokenKind::KwPublic);
}

TEST_CASE("LexerExhaustive: 035 Keyword private") {
    auto t = singleToken("private");
    CHECK_EQ(t.kind, sph::TokenKind::KwPrivate);
}

TEST_CASE("LexerExhaustive: 036 Keyword protected") {
    auto t = singleToken("protected");
    CHECK_EQ(t.kind, sph::TokenKind::KwProtected);
}

TEST_CASE("LexerExhaustive: 037 Keyword async") {
    auto t = singleToken("async");
    CHECK_EQ(t.kind, sph::TokenKind::KwAsync);
}

TEST_CASE("LexerExhaustive: 038 Keyword import") {
    auto t = singleToken("import");
    CHECK_EQ(t.kind, sph::TokenKind::KwImport);
}

TEST_CASE("LexerExhaustive: 039 Keyword echo") {
    auto t = singleToken("echo");
    CHECK_EQ(t.kind, sph::TokenKind::KwEcho);
}

TEST_CASE("LexerExhaustive: 040 Keyword this") {
    auto t = singleToken("this");
    CHECK_EQ(t.kind, sph::TokenKind::KwThis);
}

// =============================================================================
// Primitive Type Keywords (41 to 50)
// =============================================================================

TEST_CASE("LexerExhaustive: 041 Type i32") {
    auto t = singleToken("i32");
    CHECK_EQ(t.kind, sph::TokenKind::KwI32);
}

TEST_CASE("LexerExhaustive: 042 Type i64") {
    auto t = singleToken("i64");
    CHECK_EQ(t.kind, sph::TokenKind::KwI64);
}

TEST_CASE("LexerExhaustive: 043 Type f32") {
    auto t = singleToken("f32");
    CHECK_EQ(t.kind, sph::TokenKind::KwF32);
}

TEST_CASE("LexerExhaustive: 044 Type f64") {
    auto t = singleToken("f64");
    CHECK_EQ(t.kind, sph::TokenKind::KwF64);
}

TEST_CASE("LexerExhaustive: 045 Type bool") {
    auto t = singleToken("bool");
    CHECK_EQ(t.kind, sph::TokenKind::KwBool);
}

TEST_CASE("LexerExhaustive: 046 Type string") {
    auto t = singleToken("string");
    CHECK_EQ(t.kind, sph::TokenKind::KwString);
}

TEST_CASE("LexerExhaustive: 047 Type void") {
    auto t = singleToken("void");
    CHECK_EQ(t.kind, sph::TokenKind::KwVoid);
}

TEST_CASE("LexerExhaustive: 048 Literal true") {
    auto t = singleToken("true");
    CHECK_EQ(t.kind, sph::TokenKind::KwTrue);
}

TEST_CASE("LexerExhaustive: 049 Literal false") {
    auto t = singleToken("false");
    CHECK_EQ(t.kind, sph::TokenKind::KwFalse);
}

TEST_CASE("LexerExhaustive: 050 Identifier null") {
    auto t = singleToken("null");
    CHECK_EQ(t.kind, sph::TokenKind::Identifier);
    CHECK_EQ(t.text, "null");
}

// =============================================================================
// Operators and Delimiters (51 to 90)
// =============================================================================

TEST_CASE("LexerExhaustive: 051 Operator Plus +") {
    auto t = singleToken("+");
    CHECK_EQ(t.kind, sph::TokenKind::Plus);
}

TEST_CASE("LexerExhaustive: 052 Operator Minus -") {
    auto t = singleToken("-");
    CHECK_EQ(t.kind, sph::TokenKind::Minus);
}

TEST_CASE("LexerExhaustive: 053 Operator Star *") {
    auto t = singleToken("*");
    CHECK_EQ(t.kind, sph::TokenKind::Star);
}

TEST_CASE("LexerExhaustive: 054 Operator Slash /") {
    auto t = singleToken("/");
    CHECK_EQ(t.kind, sph::TokenKind::Slash);
}

TEST_CASE("LexerExhaustive: 055 Operator Percent %") {
    auto t = singleToken("%");
    CHECK_EQ(t.kind, sph::TokenKind::Percent);
}

TEST_CASE("LexerExhaustive: 056 Operator Ampersand &") {
    auto t = singleToken("&");
    CHECK_EQ(t.kind, sph::TokenKind::Amp);
}

TEST_CASE("LexerExhaustive: 057 Operator Pipe |") {
    auto t = singleToken("|");
    CHECK_EQ(t.kind, sph::TokenKind::Pipe);
}

TEST_CASE("LexerExhaustive: 058 Operator Caret ^") {
    auto t = singleToken("^");
    CHECK_EQ(t.kind, sph::TokenKind::Caret);
}

TEST_CASE("LexerExhaustive: 059 Operator Tilde ~") {
    auto t = singleToken("~");
    CHECK_EQ(t.kind, sph::TokenKind::Tilde);
}

TEST_CASE("LexerExhaustive: 060 Operator Exclaim !") {
    auto t = singleToken("!");
    CHECK_EQ(t.kind, sph::TokenKind::Exclaim);
}

TEST_CASE("LexerExhaustive: 061 Operator Equal =") {
    auto t = singleToken("=");
    CHECK_EQ(t.kind, sph::TokenKind::Equal);
}

TEST_CASE("LexerExhaustive: 062 Operator EqualEqual ==") {
    auto t = singleToken("==");
    CHECK_EQ(t.kind, sph::TokenKind::EqualEqual);
}

TEST_CASE("LexerExhaustive: 063 Operator ExclaimEqual !=") {
    auto t = singleToken("!=");
    CHECK_EQ(t.kind, sph::TokenKind::ExclaimEqual);
}

TEST_CASE("LexerExhaustive: 064 Operator Less <") {
    auto t = singleToken("<");
    CHECK_EQ(t.kind, sph::TokenKind::Less);
}

TEST_CASE("LexerExhaustive: 065 Operator LessEqual <=") {
    auto t = singleToken("<=");
    CHECK_EQ(t.kind, sph::TokenKind::LessEqual);
}

TEST_CASE("LexerExhaustive: 066 Operator Greater >") {
    auto t = singleToken(">");
    CHECK_EQ(t.kind, sph::TokenKind::Greater);
}

TEST_CASE("LexerExhaustive: 067 Operator GreaterEqual >=") {
    auto t = singleToken(">=");
    CHECK_EQ(t.kind, sph::TokenKind::GreaterEqual);
}

TEST_CASE("LexerExhaustive: 068 Operator LessLess <<") {
    auto t = singleToken("<<");
    CHECK_EQ(t.kind, sph::TokenKind::LessLess);
}

TEST_CASE("LexerExhaustive: 069 Operator GreaterGreater >>") {
    auto t = singleToken(">>");
    CHECK_EQ(t.kind, sph::TokenKind::GreaterGreater);
}

TEST_CASE("LexerExhaustive: 070 Operator AmpAmp &&") {
    auto t = singleToken("&&");
    CHECK_EQ(t.kind, sph::TokenKind::AmpAmp);
}

TEST_CASE("LexerExhaustive: 071 Operator PipePipe ||") {
    auto t = singleToken("||");
    CHECK_EQ(t.kind, sph::TokenKind::PipePipe);
}

TEST_CASE("LexerExhaustive: 072 Operator Arrow ->") {
    auto t = singleToken("->");
    CHECK_EQ(t.kind, sph::TokenKind::Arrow);
}

TEST_CASE("LexerExhaustive: 073 Operator DoubleArrow =>") {
    auto t = singleToken("=>");
    CHECK_EQ(t.kind, sph::TokenKind::DoubleArrow);
}

TEST_CASE("LexerExhaustive: 074 Operator ColonColon ::") {
    auto t = singleToken("::");
    CHECK_EQ(t.kind, sph::TokenKind::ColonColon);
}

TEST_CASE("LexerExhaustive: 075 Operator DotDot ..") {
    auto t = singleToken("..");
    CHECK_EQ(t.kind, sph::TokenKind::DotDot);
}

TEST_CASE("LexerExhaustive: 076 Operator Question ?") {
    auto t = singleToken("?");
    CHECK_EQ(t.kind, sph::TokenKind::Question);
}

TEST_CASE("LexerExhaustive: 077 Operator Question Question") {
    auto tokens = tokenize("? ?");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Question);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Question);
}

TEST_CASE("LexerExhaustive: 078 Operator Dot .") {
    auto t = singleToken(".");
    CHECK_EQ(t.kind, sph::TokenKind::Dot);
}

TEST_CASE("LexerExhaustive: 079 Delimiter Comma ,") {
    auto t = singleToken(",");
    CHECK_EQ(t.kind, sph::TokenKind::Comma);
}

TEST_CASE("LexerExhaustive: 080 Delimiter Colon :") {
    auto t = singleToken(":");
    CHECK_EQ(t.kind, sph::TokenKind::Colon);
}

TEST_CASE("LexerExhaustive: 081 Delimiter Semicolon ;") {
    auto t = singleToken(";");
    CHECK_EQ(t.kind, sph::TokenKind::Semicolon);
}

TEST_CASE("LexerExhaustive: 082 Delimiter LParen (") {
    auto t = singleToken("(");
    CHECK_EQ(t.kind, sph::TokenKind::LParen);
}

TEST_CASE("LexerExhaustive: 083 Delimiter RParen )") {
    auto t = singleToken(")");
    CHECK_EQ(t.kind, sph::TokenKind::RParen);
}

TEST_CASE("LexerExhaustive: 084 Delimiter LBrace {") {
    auto t = singleToken("{");
    CHECK_EQ(t.kind, sph::TokenKind::LBrace);
}

TEST_CASE("LexerExhaustive: 085 Delimiter RBrace }") {
    auto t = singleToken("}");
    CHECK_EQ(t.kind, sph::TokenKind::RBrace);
}

TEST_CASE("LexerExhaustive: 086 Delimiter LBracket [") {
    auto t = singleToken("[");
    CHECK_EQ(t.kind, sph::TokenKind::LBracket);
}

TEST_CASE("LexerExhaustive: 087 Delimiter RBracket ]") {
    auto t = singleToken("]");
    CHECK_EQ(t.kind, sph::TokenKind::RBracket);
}

TEST_CASE("LexerExhaustive: 088 Delimiter Hash #") {
    auto t = singleToken("#");
    CHECK_EQ(t.kind, sph::TokenKind::Hash);
}

TEST_CASE("LexerExhaustive: 089 Plus and Equal +=") {
    auto tokens = tokenize("+=");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Plus);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Equal);
}

TEST_CASE("LexerExhaustive: 090 Minus and Equal -=") {
    auto tokens = tokenize("-=");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Minus);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Equal);
}

// =============================================================================
// Literals and Numbers (91 to 125)
// =============================================================================

TEST_CASE("LexerExhaustive: 091 Simple integer 0") {
    auto t = singleToken("0");
    CHECK_EQ(t.kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(t.text, "0");
}

TEST_CASE("LexerExhaustive: 092 Positive integer 42") {
    auto t = singleToken("42");
    CHECK_EQ(t.kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(t.text, "42");
}

TEST_CASE("LexerExhaustive: 093 Large integer 1000000") {
    auto t = singleToken("1000000");
    CHECK_EQ(t.kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(t.text, "1000000");
}

TEST_CASE("LexerExhaustive: 094 Hex literal 0x0") {
    auto t = singleToken("0x0");
    CHECK_EQ(t.kind, sph::TokenKind::IntLiteral);
}

TEST_CASE("LexerExhaustive: 095 Hex literal 0xFF") {
    auto t = singleToken("0xFF");
    CHECK_EQ(t.kind, sph::TokenKind::IntLiteral);
}

TEST_CASE("LexerExhaustive: 096 Hex literal lowercase 0xabcd") {
    auto t = singleToken("0xabcd");
    CHECK_EQ(t.kind, sph::TokenKind::IntLiteral);
}

TEST_CASE("LexerExhaustive: 097 Binary literal 0b0") {
    auto t = singleToken("0b0");
    CHECK_EQ(t.kind, sph::TokenKind::IntLiteral);
}

TEST_CASE("LexerExhaustive: 098 Binary literal 0b1010") {
    auto t = singleToken("0b1010");
    CHECK_EQ(t.kind, sph::TokenKind::IntLiteral);
}

TEST_CASE("LexerExhaustive: 099 Float simple 3.14") {
    auto t = singleToken("3.14");
    CHECK_EQ(t.kind, sph::TokenKind::FloatLiteral);
    CHECK_EQ(t.text, "3.14");
}

TEST_CASE("LexerExhaustive: 100 Float zero 0.0") {
    auto t = singleToken("0.0");
    CHECK_EQ(t.kind, sph::TokenKind::FloatLiteral);
}

TEST_CASE("LexerExhaustive: 101 Float scientific 1e10") {
    auto t = singleToken("1e10");
    CHECK_EQ(t.kind, sph::TokenKind::FloatLiteral);
}

TEST_CASE("LexerExhaustive: 102 Float scientific with dot 2.5e3") {
    auto t = singleToken("2.5e3");
    CHECK_EQ(t.kind, sph::TokenKind::FloatLiteral);
}

TEST_CASE("LexerExhaustive: 103 Float negative exponent 1.2e-4") {
    auto t = singleToken("1.2e-4");
    CHECK_EQ(t.kind, sph::TokenKind::FloatLiteral);
}

TEST_CASE("LexerExhaustive: 104 String empty \"\"") {
    auto t = singleToken("\"\"");
    CHECK_EQ(t.kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(t.text, "");
}

TEST_CASE("LexerExhaustive: 105 String hello world") {
    auto t = singleToken("\"hello world\"");
    CHECK_EQ(t.kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(t.text, "hello world");
}

TEST_CASE("LexerExhaustive: 106 String with newline escape") {
    auto t = singleToken("\"hello\\nworld\"");
    CHECK_EQ(t.kind, sph::TokenKind::StringLiteral);
}

TEST_CASE("LexerExhaustive: 107 String with tab escape") {
    auto t = singleToken("\"tab\\ttab\"");
    CHECK_EQ(t.kind, sph::TokenKind::StringLiteral);
}

TEST_CASE("LexerExhaustive: 108 String with escaped quote") {
    auto t = singleToken("\"quote: \\\"inside\\\"\"");
    CHECK_EQ(t.kind, sph::TokenKind::StringLiteral);
}

TEST_CASE("LexerExhaustive: 109 String with backslash escape") {
    auto t = singleToken("\"path\\\\to\\\\file\"");
    CHECK_EQ(t.kind, sph::TokenKind::StringLiteral);
}

TEST_CASE("LexerExhaustive: 110 Single quoted string empty") {
    auto t = singleToken("''");
    CHECK_EQ(t.kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(t.text, "");
}

TEST_CASE("LexerExhaustive: 111 Single quoted string text") {
    auto t = singleToken("'santient php'");
    CHECK_EQ(t.kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(t.text, "santient php");
}

TEST_CASE("LexerExhaustive: 112 PHP variable $x") {
    auto t = singleToken("$x");
    CHECK_EQ(t.kind, sph::TokenKind::Identifier);
    CHECK_EQ(t.text, "$x");
}

TEST_CASE("LexerExhaustive: 113 PHP variable $long_identifier_123") {
    auto t = singleToken("$long_identifier_123");
    CHECK_EQ(t.kind, sph::TokenKind::Identifier);
    CHECK_EQ(t.text, "$long_identifier_123");
}

TEST_CASE("LexerExhaustive: 114 Standard identifier myFunc") {
    auto t = singleToken("myFunc");
    CHECK_EQ(t.kind, sph::TokenKind::Identifier);
    CHECK_EQ(t.text, "myFunc");
}

TEST_CASE("LexerExhaustive: 115 Uppercase identifier MyType") {
    auto t = singleToken("MyType");
    CHECK_EQ(t.kind, sph::TokenKind::Identifier);
    CHECK_EQ(t.text, "MyType");
}

TEST_CASE("LexerExhaustive: 116 Underscore leading identifier _secret") {
    auto t = singleToken("_secret");
    CHECK_EQ(t.kind, sph::TokenKind::Identifier);
    CHECK_EQ(t.text, "_secret");
}

TEST_CASE("LexerExhaustive: 117 Identifier with numbers v2_beta3") {
    auto t = singleToken("v2_beta3");
    CHECK_EQ(t.kind, sph::TokenKind::Identifier);
    CHECK_EQ(t.text, "v2_beta3");
}

TEST_CASE("LexerExhaustive: 118 PHP open tag <?php") {
    auto tokens = tokenize("<?php");
    CHECK(!tokens.empty());
    CHECK_EQ(tokens[0].kind, sph::TokenKind::PhpOpenTag);
}

TEST_CASE("LexerExhaustive: 119 Double colon scope resolution") {
    auto tokens = tokenize("Foo::bar");
    CHECK_EQ(tokens.size(), 4u); // Foo, ::, bar, EOF
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::ColonColon);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Identifier);
}

TEST_CASE("LexerExhaustive: 120 Member arrow access") {
    auto tokens = tokenize("$user->name");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Arrow);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Identifier);
}

TEST_CASE("LexerExhaustive: 121 Double arrow hash map key value") {
    auto tokens = tokenize("\"key\" => 123");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::StringLiteral);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::DoubleArrow);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::IntLiteral);
}

TEST_CASE("LexerExhaustive: 122 Array append brackets []") {
    auto tokens = tokenize("$arr[]");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::LBracket);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::RBracket);
}

TEST_CASE("LexerExhaustive: 123 Slice range .. in brackets") {
    auto tokens = tokenize("arr[1..5]");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::LBracket);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::DotDot);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::RBracket);
}

TEST_CASE("LexerExhaustive: 124 Attribute syntax #[inline]") {
    auto tokens = tokenize("#[inline]");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Hash);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::LBracket);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::RBracket);
}

TEST_CASE("LexerExhaustive: 125 Try operator tokenizing ?") {
    auto tokens = tokenize("get_user()?");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Identifier);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::LParen);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::RParen);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::Question);
}

// =============================================================================
// Comments, Whitespace, and Sequences (126 to 150)
// =============================================================================

TEST_CASE("LexerExhaustive: 126 Single-line comment ignored") {
    auto tokens = tokenize("// this is a comment\n42");
    CHECK_EQ(tokens.size(), 2u); // 42, EOF
    CHECK_EQ(tokens[0].kind, sph::TokenKind::IntLiteral);
}

TEST_CASE("LexerExhaustive: 127 Multi-line block comment ignored") {
    auto tokens = tokenize("/* block comment */ 100");
    CHECK_EQ(tokens.size(), 2u);
    CHECK_EQ(tokens[0].kind, sph::TokenKind::IntLiteral);
}

TEST_CASE("LexerExhaustive: 128 Multiline comment across lines") {
    auto tokens = tokenize("/* line 1\n line 2\n line 3 */ let x = 1;");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::KwLet);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Identifier);
}

TEST_CASE("LexerExhaustive: 129 Comment at end of file") {
    auto tokens = tokenize("return 0; // exit code");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::KwReturn);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Semicolon);
}

TEST_CASE("LexerExhaustive: 130 Multiple whitespace characters") {
    auto tokens = tokenize("   \t   \r\n   42   ");
    CHECK_EQ(tokens.size(), 2u);
    CHECK_EQ(tokens[0].text, "42");
}

TEST_CASE("LexerExhaustive: 131 Equality operator sequence ==") {
    auto tokens = tokenize("$a == $b");
    CHECK_EQ(tokens[1].kind, sph::TokenKind::EqualEqual);
}

TEST_CASE("LexerExhaustive: 132 Inequality operator !=") {
    auto tokens = tokenize("$a != $b");
    CHECK_EQ(tokens[1].kind, sph::TokenKind::ExclaimEqual);
}

TEST_CASE("LexerExhaustive: 133 Double Question sequence ? ?") {
    auto tokens = tokenize("? ?");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Question);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Question);
}

TEST_CASE("LexerExhaustive: 134 Two dots sequence ..") {
    auto tokens = tokenize("..");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::DotDot);
}

TEST_CASE("LexerExhaustive: 135 Multiplication and Equal * =") {
    auto tokens = tokenize("x * = 2");
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Star);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Equal);
}

TEST_CASE("LexerExhaustive: 136 Division and Equal / =") {
    auto tokens = tokenize("x / = 2");
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Slash);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Equal);
}

TEST_CASE("LexerExhaustive: 137 Modulo and Equal % =") {
    auto tokens = tokenize("x % = 2");
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Percent);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Equal);
}

TEST_CASE("LexerExhaustive: 138 Bitwise AND and Equal & =") {
    auto tokens = tokenize("x & = 2");
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Amp);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Equal);
}

TEST_CASE("LexerExhaustive: 139 Bitwise OR and Equal | =") {
    auto tokens = tokenize("x | = 2");
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Pipe);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Equal);
}

TEST_CASE("LexerExhaustive: 140 Bitwise XOR and Equal ^ =") {
    auto tokens = tokenize("x ^ = 2");
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Caret);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Equal);
}

TEST_CASE("LexerExhaustive: 141 Shift left and Equal << =") {
    auto tokens = tokenize("x << = 2");
    CHECK_EQ(tokens[1].kind, sph::TokenKind::LessLess);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Equal);
}

TEST_CASE("LexerExhaustive: 142 Shift right and Equal >> =") {
    auto tokens = tokenize("x >> = 2");
    CHECK_EQ(tokens[1].kind, sph::TokenKind::GreaterGreater);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::Equal);
}

TEST_CASE("LexerExhaustive: 143 Bitwise NOT ~0") {
    auto tokens = tokenize("~0");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Tilde);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::IntLiteral);
}

TEST_CASE("LexerExhaustive: 144 Address-of &val") {
    auto tokens = tokenize("&val");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Amp);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Identifier);
}

TEST_CASE("LexerExhaustive: 145 Pointer dereference *ptr") {
    auto tokens = tokenize("*ptr");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::Star);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Identifier);
}

TEST_CASE("LexerExhaustive: 146 Nested generics tokens Vec<Option<i32>>") {
    auto tokens = tokenize("Vec<Option<i32>>");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::KwVec);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Less);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::KwOption);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::Less);
    CHECK_EQ(tokens[4].kind, sph::TokenKind::KwI32);
    CHECK_EQ(tokens[5].kind, sph::TokenKind::GreaterGreater);
}

TEST_CASE("LexerExhaustive: 147 Match arm tokenization 1 | 2 =>") {
    auto tokens = tokenize("1 | 2 =>");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::Pipe);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::DoubleArrow);
}

TEST_CASE("LexerExhaustive: 148 Range match tokenization 10..20 =>") {
    auto tokens = tokenize("10..20 =>");
    CHECK_EQ(tokens[0].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[1].kind, sph::TokenKind::DotDot);
    CHECK_EQ(tokens[2].kind, sph::TokenKind::IntLiteral);
    CHECK_EQ(tokens[3].kind, sph::TokenKind::DoubleArrow);
}

TEST_CASE("LexerExhaustive: 149 Unterminated string error diagnostic") {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    uint32_t fileId = sm.addSource("test.sph", "\"unterminated string");
    sph::Lexer lexer(fileId, sm, &emitter);
    lexer.tokenizeAll();
    CHECK(emitter.hasErrors());
}

TEST_CASE("LexerExhaustive: 150 End of file token emitted") {
    auto tokens = tokenize("");
    CHECK(!tokens.empty());
    CHECK_EQ(tokens.back().kind, sph::TokenKind::EndOfFile);
}
