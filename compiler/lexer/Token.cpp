#include "Token.hpp"

namespace sph {

const char* tokenKindToString(TokenKind kind) noexcept {
    switch (kind) {
        case TokenKind::EndOfFile:    return "EndOfFile";
        case TokenKind::Invalid:      return "Invalid";
        case TokenKind::PhpOpenTag:   return "<?php";
        case TokenKind::KwFn:         return "fn";
        case TokenKind::KwLet:        return "let";
        case TokenKind::KwMut:        return "mut";
        case TokenKind::KwReturn:     return "return";
        case TokenKind::KwIf:         return "if";
        case TokenKind::KwElse:       return "else";
        case TokenKind::KwWhile:      return "while";
        case TokenKind::KwForeach:    return "foreach";
        case TokenKind::KwBreak:      return "break";
        case TokenKind::KwContinue:   return "continue";
        case TokenKind::KwStruct:     return "struct";
        case TokenKind::KwEnum:       return "enum";
        case TokenKind::KwImport:     return "import";
        case TokenKind::KwClass:      return "class";
        case TokenKind::KwThis:       return "this";
        case TokenKind::KwInterface:  return "interface";
        case TokenKind::KwImplements: return "implements";
        case TokenKind::KwUnsafe:     return "unsafe";
        case TokenKind::KwExtern:     return "extern";
        case TokenKind::KwAs:         return "as";
        case TokenKind::KwRc:         return "Rc";
        case TokenKind::KwBox:        return "Box";
        case TokenKind::KwArena:      return "Arena";
        case TokenKind::KwVec:        return "Vec";
        case TokenKind::KwEcho:       return "echo";
        case TokenKind::KwDynamic:    return "dynamic";
        case TokenKind::KwNew:        return "new";
        case TokenKind::KwFunction:   return "function";
        case TokenKind::KwPublic:     return "public";
        case TokenKind::KwPrivate:    return "private";
        case TokenKind::KwProtected:  return "protected";
        case TokenKind::KwTry:        return "try";
        case TokenKind::KwCatch:      return "catch";
        case TokenKind::KwFinally:    return "finally";
        case TokenKind::KwThrow:      return "throw";
        case TokenKind::KwAsync:      return "async";
        case TokenKind::KwAwait:      return "await";
        case TokenKind::KwI32:        return "i32";
        case TokenKind::KwI64:        return "i64";
        case TokenKind::KwF32:        return "f32";
        case TokenKind::KwF64:        return "f64";
        case TokenKind::KwBool:       return "bool";
        case TokenKind::KwString:     return "string";
        case TokenKind::KwVoid:       return "void";
        case TokenKind::KwTrue:       return "true";
        case TokenKind::KwFalse:      return "false";
        case TokenKind::Identifier:   return "Identifier";
        case TokenKind::IntLiteral:   return "IntLiteral";
        case TokenKind::FloatLiteral: return "FloatLiteral";
        case TokenKind::StringLiteral:return "StringLiteral";
        case TokenKind::Plus:         return "+";
        case TokenKind::Minus:        return "-";
        case TokenKind::Star:         return "*";
        case TokenKind::Slash:        return "/";
        case TokenKind::Percent:      return "%";
        case TokenKind::Equal:        return "=";
        case TokenKind::EqualEqual:   return "==";
        case TokenKind::DoubleArrow:  return "=>";
        case TokenKind::Arrow:        return "->";
        case TokenKind::ExclaimEqual: return "!=";
        case TokenKind::Less:         return "<";
        case TokenKind::LessEqual:    return "<=";
        case TokenKind::LessLess:     return "<<";
        case TokenKind::Greater:      return ">";
        case TokenKind::GreaterEqual: return ">=";
        case TokenKind::GreaterGreater: return ">>";
        case TokenKind::Amp:          return "&";
        case TokenKind::AmpAmp:       return "&&";
        case TokenKind::Pipe:         return "|";
        case TokenKind::PipePipe:     return "||";
        case TokenKind::Caret:        return "^";
        case TokenKind::Tilde:        return "~";
        case TokenKind::Exclaim:      return "!";
        case TokenKind::Semicolon:    return ";";
        case TokenKind::Colon:        return ":";
        case TokenKind::ColonColon:   return "::";
        case TokenKind::Comma:        return ",";
        case TokenKind::Dot:          return ".";
        case TokenKind::DotDot:       return "..";
        case TokenKind::LParen:       return "(";
        case TokenKind::RParen:       return ")";
        case TokenKind::LBrace:       return "{";
        case TokenKind::RBrace:       return "}";
        case TokenKind::LBracket:     return "[";
        case TokenKind::RBracket:     return "]";
        case TokenKind::Hash:         return "#";
    }
    return "Unknown";
}

} // namespace sph

