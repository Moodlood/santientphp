#include "Lexer.hpp"
#include <cctype>
#include <string>
#include <unordered_map>

namespace sph {

namespace {

const std::unordered_map<std::string_view, TokenKind> kKeywords = {
    {"fn",     TokenKind::KwFn},
    {"let",    TokenKind::KwLet},
    {"mut",    TokenKind::KwMut},
    {"return", TokenKind::KwReturn},
    {"if",     TokenKind::KwIf},
    {"else",   TokenKind::KwElse},
    {"while",      TokenKind::KwWhile},
    {"foreach",    TokenKind::KwForeach},
    {"break",      TokenKind::KwBreak},
    {"continue",   TokenKind::KwContinue},
    {"struct",     TokenKind::KwStruct},
    {"enum",   TokenKind::KwEnum},
    {"import", TokenKind::KwImport},
    {"class",      TokenKind::KwClass},
    {"this",       TokenKind::KwThis},
    {"interface",  TokenKind::KwInterface},
    {"implements", TokenKind::KwImplements},
    {"unsafe",     TokenKind::KwUnsafe},
    {"extern",     TokenKind::KwExtern},
    {"as",         TokenKind::KwAs},
    {"Rc",         TokenKind::KwRc},
    {"Box",        TokenKind::KwBox},
    {"Arena",      TokenKind::KwArena},
    {"Vec",        TokenKind::KwVec},
    {"echo",       TokenKind::KwEcho},
    {"dynamic",    TokenKind::KwDynamic},
    {"new",        TokenKind::KwNew},
    {"function",   TokenKind::KwFunction},
    {"public",     TokenKind::KwPublic},
    {"private",    TokenKind::KwPrivate},
    {"protected",  TokenKind::KwProtected},
    {"try",        TokenKind::KwTry},
    {"catch",      TokenKind::KwCatch},
    {"finally",    TokenKind::KwFinally},
    {"throw",      TokenKind::KwThrow},
    {"async",      TokenKind::KwAsync},
    {"await",      TokenKind::KwAwait},
    {"i32",    TokenKind::KwI32},
    {"i64",    TokenKind::KwI64},
    {"f32",    TokenKind::KwF32},
    {"f64",    TokenKind::KwF64},
    {"bool",   TokenKind::KwBool},
    {"string", TokenKind::KwString},
    {"void",   TokenKind::KwVoid},
    {"true",   TokenKind::KwTrue},
    {"false",  TokenKind::KwFalse},
};

} // namespace

Lexer::Lexer(uint32_t fileId, const SourceManager& sourceManager, DiagnosticEmitter* diagnostics)
    : m_fileId(fileId), m_sourceManager(sourceManager), m_diagnostics(diagnostics) {
    const SourceFile* file = m_sourceManager.getFile(m_fileId);
    if (file) {
        m_content = file->getContent();
    }
}

char Lexer::peek() const noexcept {
    if (isAtEnd()) return '\0';
    return m_content[m_cursor];
}

char Lexer::peekNext() const noexcept {
    if (m_cursor + 1 >= m_content.size()) return '\0';
    return m_content[m_cursor + 1];
}

char Lexer::advance() noexcept {
    if (isAtEnd()) return '\0';
    return m_content[m_cursor++];
}

bool Lexer::match(char expected) noexcept {
    if (isAtEnd() || m_content[m_cursor] != expected) return false;
    ++m_cursor;
    return true;
}

SourceLocation Lexer::makeLocation(uint32_t offset) const noexcept {
    return m_sourceManager.getLocation(m_fileId, offset);
}

SourceSpan Lexer::makeSpan(uint32_t startOffset, uint32_t endOffset) const noexcept {
    return SourceSpan(makeLocation(startOffset), makeLocation(endOffset));
}

void Lexer::error(uint32_t startOffset, uint32_t endOffset, std::string message) {
    if (m_diagnostics) {
        m_diagnostics->emit(Diagnostic::makeError(makeSpan(startOffset, endOffset), std::move(message)));
    }
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }

        if (c == '/') {
            if (peekNext() == '/') {
                // Line comment: skip until newline or EOF
                advance(); // '/'
                advance(); // '/'
                while (!isAtEnd() && peek() != '\n') {
                    advance();
                }
                continue;
            }
            if (peekNext() == '*') {
                // Block comment
                uint32_t startOffset = m_cursor;
                advance(); // '/'
                advance(); // '*'
                bool closed = false;
                while (!isAtEnd()) {
                    if (peek() == '*' && peekNext() == '/') {
                        advance(); // '*'
                        advance(); // '/'
                        closed = true;
                        break;
                    }
                    advance();
                }
                if (!closed) {
                    error(startOffset, m_cursor, "unterminated block comment");
                }
                continue;
            }
        }

        break;
    }
}

Token Lexer::nextToken() {
    skipWhitespaceAndComments();

    if (isAtEnd()) {
        return Token(TokenKind::EndOfFile, makeSpan(m_cursor, m_cursor), "");
    }

    uint32_t startOffset = m_cursor;

    // Check for PHP opening tag <?php at start of file
    if (!m_seenOpeningTag) {
        m_seenOpeningTag = true;
        if (m_cursor + 5 <= m_content.size() && m_content.substr(m_cursor, 5) == "<?php") {
            m_cursor += 5;
            return Token(TokenKind::PhpOpenTag, makeSpan(startOffset, m_cursor), "<?php");
        }
        // If file does not start with <?php, emit diagnostic
        error(startOffset, startOffset + 1, "expected '<?php' at beginning of file");
    }

    char c = advance();

    // Identifiers & Keywords (including PHP $ variables)
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
        return scanIdentifierOrKeyword(startOffset);
    }

    // Numbers
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return scanNumber(startOffset);
    }

    // Strings
    if (c == '"' || c == '\'') {
        return scanString(c, startOffset);
    }

    // Operators and Delimiters
    switch (c) {
        case '+': return Token(TokenKind::Plus, makeSpan(startOffset, m_cursor), "+");
        case '-':
            if (match('>')) return Token(TokenKind::Arrow, makeSpan(startOffset, m_cursor), "->");
            return Token(TokenKind::Minus, makeSpan(startOffset, m_cursor), "-");
        case '*': return Token(TokenKind::Star, makeSpan(startOffset, m_cursor), "*");
        case '/': return Token(TokenKind::Slash, makeSpan(startOffset, m_cursor), "/");
        case '%': return Token(TokenKind::Percent, makeSpan(startOffset, m_cursor), "%");
        case ';': return Token(TokenKind::Semicolon, makeSpan(startOffset, m_cursor), ";");
        case ':':
            if (match(':')) return Token(TokenKind::ColonColon, makeSpan(startOffset, m_cursor), "::");
            return Token(TokenKind::Colon, makeSpan(startOffset, m_cursor), ":");
        case ',': return Token(TokenKind::Comma, makeSpan(startOffset, m_cursor), ",");
        case '.':
            if (match('.')) return Token(TokenKind::DotDot, makeSpan(startOffset, m_cursor), "..");
            return Token(TokenKind::Dot, makeSpan(startOffset, m_cursor), ".");
        case '(': return Token(TokenKind::LParen, makeSpan(startOffset, m_cursor), "(");
        case ')': return Token(TokenKind::RParen, makeSpan(startOffset, m_cursor), ")");
        case '{': return Token(TokenKind::LBrace, makeSpan(startOffset, m_cursor), "{");
        case '}': return Token(TokenKind::RBrace, makeSpan(startOffset, m_cursor), "}");
        case '[': return Token(TokenKind::LBracket, makeSpan(startOffset, m_cursor), "[");
        case ']': return Token(TokenKind::RBracket, makeSpan(startOffset, m_cursor), "]");

        case '=':
            if (match('=')) return Token(TokenKind::EqualEqual, makeSpan(startOffset, m_cursor), "==");
            if (match('>')) return Token(TokenKind::DoubleArrow, makeSpan(startOffset, m_cursor), "=>");
            return Token(TokenKind::Equal, makeSpan(startOffset, m_cursor), "=");

        case '!':
            if (match('=')) return Token(TokenKind::ExclaimEqual, makeSpan(startOffset, m_cursor), "!=");
            return Token(TokenKind::Exclaim, makeSpan(startOffset, m_cursor), "!");

        case '<':
            if (match('<')) return Token(TokenKind::LessLess, makeSpan(startOffset, m_cursor), "<<");
            if (match('=')) return Token(TokenKind::LessEqual, makeSpan(startOffset, m_cursor), "<=");
            return Token(TokenKind::Less, makeSpan(startOffset, m_cursor), "<");

        case '>':
            if (match('>')) return Token(TokenKind::GreaterGreater, makeSpan(startOffset, m_cursor), ">>");
            if (match('=')) return Token(TokenKind::GreaterEqual, makeSpan(startOffset, m_cursor), ">=");
            return Token(TokenKind::Greater, makeSpan(startOffset, m_cursor), ">");

        case '&':
            if (match('&')) return Token(TokenKind::AmpAmp, makeSpan(startOffset, m_cursor), "&&");
            return Token(TokenKind::Amp, makeSpan(startOffset, m_cursor), "&");

        case '|':
            if (match('|')) return Token(TokenKind::PipePipe, makeSpan(startOffset, m_cursor), "||");
            return Token(TokenKind::Pipe, makeSpan(startOffset, m_cursor), "|");

        case '^':
            return Token(TokenKind::Caret, makeSpan(startOffset, m_cursor), "^");

        case '~':
            return Token(TokenKind::Tilde, makeSpan(startOffset, m_cursor), "~");

        case '#':
            return Token(TokenKind::Hash, makeSpan(startOffset, m_cursor), "#");

        default:
            error(startOffset, m_cursor, std::string("unexpected character '") + c + "'");
            return Token(TokenKind::Invalid, makeSpan(startOffset, m_cursor), std::string(1, c));
    }
}

Token Lexer::scanIdentifierOrKeyword(uint32_t startOffset) {
    while (!isAtEnd()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            advance();
        } else {
            break;
        }
    }

    std::string_view lexeme = m_content.substr(startOffset, m_cursor - startOffset);
    if (lexeme == "$") {
        error(startOffset, m_cursor, "expected variable name after '$'");
        return Token(TokenKind::Invalid, makeSpan(startOffset, m_cursor), "$");
    }
    auto it = kKeywords.find(lexeme);
    TokenKind kind = (it != kKeywords.end()) ? it->second : TokenKind::Identifier;

    return Token(kind, makeSpan(startOffset, m_cursor), std::string(lexeme));
}

Token Lexer::scanNumber(uint32_t startOffset) {
    // Hexadecimal: 0x...
    if (m_content[startOffset] == '0' && (peek() == 'x' || peek() == 'X')) {
        advance(); // 'x'
        uint32_t hexDigitsStart = m_cursor;
        while (!isAtEnd() && std::isxdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
        if (m_cursor == hexDigitsStart) {
            error(startOffset, m_cursor, "expected hexadecimal digits after '0x'");
            return Token(TokenKind::Invalid, makeSpan(startOffset, m_cursor), std::string(m_content.substr(startOffset, m_cursor - startOffset)));
        }
        return Token(TokenKind::IntLiteral, makeSpan(startOffset, m_cursor), std::string(m_content.substr(startOffset, m_cursor - startOffset)));
    }

    // Binary: 0b...
    if (m_content[startOffset] == '0' && (peek() == 'b' || peek() == 'B')) {
        advance(); // 'b'
        uint32_t binDigitsStart = m_cursor;
        while (!isAtEnd() && (peek() == '0' || peek() == '1')) {
            advance();
        }
        if (m_cursor == binDigitsStart) {
            error(startOffset, m_cursor, "expected binary digits after '0b'");
            return Token(TokenKind::Invalid, makeSpan(startOffset, m_cursor), std::string(m_content.substr(startOffset, m_cursor - startOffset)));
        }
        return Token(TokenKind::IntLiteral, makeSpan(startOffset, m_cursor), std::string(m_content.substr(startOffset, m_cursor - startOffset)));
    }

    // Decimal digits
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    // Check for float dot
    bool isFloat = false;
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        isFloat = true;
        advance(); // '.'
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    // Check for scientific exponent
    if (peek() == 'e' || peek() == 'E') {
        isFloat = true;
        advance(); // 'e'
        if (peek() == '+' || peek() == '-') {
            advance();
        }
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    std::string_view lexeme = m_content.substr(startOffset, m_cursor - startOffset);
    TokenKind kind = isFloat ? TokenKind::FloatLiteral : TokenKind::IntLiteral;
    return Token(kind, makeSpan(startOffset, m_cursor), std::string(lexeme));
}

Token Lexer::scanString(char quoteChar, uint32_t startOffset) {
    std::string value;
    bool closed = false;

    while (!isAtEnd()) {
        char c = advance();
        if (c == quoteChar) {
            closed = true;
            break;
        }

        if (c == '\n' || c == '\r') {
            error(startOffset, m_cursor, "unterminated string literal");
            return Token(TokenKind::Invalid, makeSpan(startOffset, m_cursor), std::string(m_content.substr(startOffset, m_cursor - startOffset)));
        }

        if (c == '\\') {
            if (isAtEnd()) {
                break;
            }
            char escape = advance();
            switch (escape) {
                case 'n':  value.push_back('\n'); break;
                case 't':  value.push_back('\t'); break;
                case 'r':  value.push_back('\r'); break;
                case '\\': value.push_back('\\'); break;
                case '\'': value.push_back('\''); break;
                case '"':  value.push_back('"');  break;
                case '0':  value.push_back('\0'); break;
                default:
                    error(m_cursor - 2, m_cursor, std::string("unknown escape sequence '\\") + escape + "'");
                    value.push_back(escape);
                    break;
            }
        } else {
            value.push_back(c);
        }
    }

    if (!closed) {
        error(startOffset, m_cursor, "unterminated string literal");
        return Token(TokenKind::Invalid, makeSpan(startOffset, m_cursor), std::string(m_content.substr(startOffset, m_cursor - startOffset)));
    }

    return Token(TokenKind::StringLiteral, makeSpan(startOffset, m_cursor), std::move(value));
}

std::vector<Token> Lexer::tokenizeAll() {
    std::vector<Token> tokens;
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.is(TokenKind::EndOfFile)) {
            break;
        }
    }
    return tokens;
}

} // namespace sph

