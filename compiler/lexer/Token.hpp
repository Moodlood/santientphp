#pragma once

#include "compiler/source/SourceLocation.hpp"
#include <ostream>
#include <string>
#include <string_view>

namespace sph {

enum class TokenKind {
    EndOfFile,
    Invalid,

    // Opening tag
    PhpOpenTag, // <?php

    // Keywords
    KwFn,
    KwLet,
    KwMut,
    KwReturn,
    KwIf,
    KwElse,
    KwWhile,
    KwForeach,
    KwBreak,
    KwContinue,
    KwStruct,
    KwEnum,
    KwImport,
    KwClass,
    KwThis,
    KwInterface,
    KwImplements,
    KwUnsafe,
    KwExtern,
    KwAs,
    KwRc,
    KwBox,
    KwArena,
    KwVec,
    KwEcho,
    KwDynamic,
    KwNew,
    KwFunction,
    KwPublic,
    KwPrivate,
    KwProtected,
    KwTry,
    KwCatch,
    KwFinally,
    KwThrow,
    KwAsync,
    KwAwait,

    // Primitive Type Keywords
    KwI32,
    KwI64,
    KwF32,
    KwF64,
    KwBool,
    KwString,
    KwVoid,

    // Boolean Literal Keywords
    KwTrue,
    KwFalse,

    // Identifiers
    Identifier,

    // Literals
    IntLiteral,
    FloatLiteral,
    StringLiteral,

    // Operators
    Plus,          // +
    Minus,         // -
    Star,          // *
    Slash,         // /
    Percent,       // %
    Equal,         // =
    EqualEqual,    // ==
    DoubleArrow,   // =>
    Arrow,         // ->
    ExclaimEqual,  // !=
    Less,          // <
    LessEqual,     // <=
    LessLess,      // <<
    Greater,       // >
    GreaterEqual,  // >=
    GreaterGreater,// >>
    Amp,           // &
    AmpAmp,        // &&
    Pipe,          // |
    PipePipe,      // ||
    Caret,         // ^
    Tilde,         // ~
    Exclaim,       // !

    // Punctuation & Delimiters
    Semicolon,     // ;
    Colon,         // :
    ColonColon,    // ::
    Comma,         // ,
    Dot,           // .
    DotDot,        // ..
    LParen,        // (
    RParen,        // )
    LBrace,        // {
    RBrace,        // }
    LBracket,      // [
    RBracket,      // ]
    Hash           // #
};

[[nodiscard]] const char* tokenKindToString(TokenKind kind) noexcept;

inline std::ostream& operator<<(std::ostream& os, TokenKind kind) {
    return os << tokenKindToString(kind);
}

struct Token {
    TokenKind kind = TokenKind::Invalid;
    SourceSpan span;
    std::string text; // Lexeme or unescaped string value

    Token() = default;
    Token(TokenKind k, SourceSpan sp, std::string txt)
        : kind(k), span(sp), text(std::move(txt)) {}

    [[nodiscard]] bool is(TokenKind k) const noexcept { return kind == k; }
    [[nodiscard]] bool isNot(TokenKind k) const noexcept { return kind != k; }
};

} // namespace sph
