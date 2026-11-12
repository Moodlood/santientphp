#include "compiler/formatter/Formatter.hpp"
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace sph {

namespace {

enum class RawTokenKind {
    PhpOpenTag,
    Whitespace,
    Newline,
    CommentLine,
    CommentBlock,
    StringLit,
    Identifier,
    Number,
    Operator,
    Punctuation,
    Other
};

struct RawToken {
    RawTokenKind kind;
    std::string text;
};

// Tokenizer that preserves comments and newlines
std::vector<RawToken> tokenizeForFormat(std::string_view source) {
    std::vector<RawToken> tokens;
    size_t i = 0;
    const size_t len = source.size();

    auto peek = [&](size_t offset = 0) -> char {
        if (i + offset >= len) return '\0';
        return source[i + offset];
    };

    while (i < len) {
        char c = source[i];

        // PHP Opening Tag: <?php
        if (c == '<' && i + 4 < len && source.substr(i, 5) == "<?php") {
            tokens.push_back({RawTokenKind::PhpOpenTag, "<?php"});
            i += 5;
            continue;
        }

        // Newline
        if (c == '\n') {
            tokens.push_back({RawTokenKind::Newline, "\n"});
            i++;
            continue;
        }
        if (c == '\r') {
            if (i + 1 < len && source[i + 1] == '\n') {
                i++;
            }
            tokens.push_back({RawTokenKind::Newline, "\n"});
            i++;
            continue;
        }

        // Whitespace (horizontal)
        if (c == ' ' || c == '\t') {
            size_t start = i;
            while (i < len && (source[i] == ' ' || source[i] == '\t')) {
                i++;
            }
            tokens.push_back({RawTokenKind::Whitespace, std::string(source.substr(start, i - start))});
            continue;
        }

        // Line comment: //
        if (c == '/' && peek(1) == '/') {
            size_t start = i;
            while (i < len && source[i] != '\n' && source[i] != '\r') {
                i++;
            }
            tokens.push_back({RawTokenKind::CommentLine, std::string(source.substr(start, i - start))});
            continue;
        }

        // Block comment: /* ... */
        if (c == '/' && peek(1) == '*') {
            size_t start = i;
            i += 2;
            while (i < len) {
                if (source[i] == '*' && peek(1) == '/') {
                    i += 2;
                    break;
                }
                i++;
            }
            tokens.push_back({RawTokenKind::CommentBlock, std::string(source.substr(start, i - start))});
            continue;
        }

        // String literals: "..." or '...'
        if (c == '"' || c == '\'') {
            char quote = c;
            size_t start = i;
            i++;
            while (i < len && source[i] != quote) {
                if (source[i] == '\\' && i + 1 < len) {
                    i += 2; // skip escaped char
                } else {
                    i++;
                }
            }
            if (i < len && source[i] == quote) {
                i++; // closing quote
            }
            tokens.push_back({RawTokenKind::StringLit, std::string(source.substr(start, i - start))});
            continue;
        }

        // Numbers: digits or hex
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && std::isdigit(static_cast<unsigned char>(peek(1))))) {
            size_t start = i;
            if (c == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
                i += 2;
                while (i < len && std::isxdigit(static_cast<unsigned char>(source[i]))) {
                    i++;
                }
            } else {
                bool hasDot = (c == '.');
                i++;
                while (i < len) {
                    char ch = source[i];
                    if (std::isdigit(static_cast<unsigned char>(ch))) {
                        i++;
                    } else if (ch == '.' && !hasDot && peek(1) != '.') {
                        hasDot = true;
                        i++;
                    } else if (ch == 'f' || ch == 'u' || ch == 'l' || ch == 'i') {
                        // type suffixes like 100i32 or 1.0f
                        while (i < len && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) {
                            i++;
                        }
                        break;
                    } else {
                        break;
                    }
                }
            }
            tokens.push_back({RawTokenKind::Number, std::string(source.substr(start, i - start))});
            continue;
        }

        // Identifiers and keywords (including PHP variables like $var)
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            size_t start = i;
            i++;
            while (i < len && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) {
                i++;
            }
            tokens.push_back({RawTokenKind::Identifier, std::string(source.substr(start, i - start))});
            continue;
        }

        // Multi-character operators
        // ===, !==, ==, !=, <=, >=, <<, >>, &&, ||, =>, ->, ::, ..
        if (c == '=' && peek(1) == '=' && peek(2) == '=') {
            tokens.push_back({RawTokenKind::Operator, "==="});
            i += 3;
            continue;
        }
        if (c == '!' && peek(1) == '=' && peek(2) == '=') {
            tokens.push_back({RawTokenKind::Operator, "!=="});
            i += 3;
            continue;
        }
        if (c == '=' && peek(1) == '=') {
            tokens.push_back({RawTokenKind::Operator, "=="});
            i += 2;
            continue;
        }
        if (c == '!' && peek(1) == '=') {
            tokens.push_back({RawTokenKind::Operator, "!="});
            i += 2;
            continue;
        }
        if (c == '<' && peek(1) == '=') {
            tokens.push_back({RawTokenKind::Operator, "<="});
            i += 2;
            continue;
        }
        if (c == '>' && peek(1) == '=') {
            tokens.push_back({RawTokenKind::Operator, ">="});
            i += 2;
            continue;
        }
        if (c == '<' && peek(1) == '<') {
            tokens.push_back({RawTokenKind::Operator, "<<"});
            i += 2;
            continue;
        }
        if (c == '>' && peek(1) == '>') {
            tokens.push_back({RawTokenKind::Operator, ">>"});
            i += 2;
            continue;
        }
        if (c == '&' && peek(1) == '&') {
            tokens.push_back({RawTokenKind::Operator, "&&"});
            i += 2;
            continue;
        }
        if (c == '|' && peek(1) == '|') {
            tokens.push_back({RawTokenKind::Operator, "||"});
            i += 2;
            continue;
        }
        if (c == '=' && peek(1) == '>') {
            tokens.push_back({RawTokenKind::Operator, "=>"});
            i += 2;
            continue;
        }
        if (c == '-' && peek(1) == '>') {
            tokens.push_back({RawTokenKind::Operator, "->"});
            i += 2;
            continue;
        }
        if (c == ':' && peek(1) == ':') {
            tokens.push_back({RawTokenKind::Operator, "::"});
            i += 2;
            continue;
        }
        if (c == '.' && peek(1) == '.') {
            tokens.push_back({RawTokenKind::Operator, ".."});
            i += 2;
            continue;
        }
        if ((c == '+' || c == '-' || c == '*' || c == '/' || c == '%') && peek(1) == '=') {
            std::string op(1, c);
            op += '=';
            tokens.push_back({RawTokenKind::Operator, op});
            i += 2;
            continue;
        }

        // Single character operators and punctuation
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '=' ||
            c == '<' || c == '>' || c == '&' || c == '|' || c == '^' || c == '~' || c == '!') {
            tokens.push_back({RawTokenKind::Operator, std::string(1, c)});
            i++;
            continue;
        }

        if (c == ';' || c == ':' || c == ',' || c == '.' || c == '(' || c == ')' ||
            c == '{' || c == '}' || c == '[' || c == ']' || c == '#' || c == '?') {
            tokens.push_back({RawTokenKind::Punctuation, std::string(1, c)});
            i++;
            continue;
        }

        // Any other character
        tokens.push_back({RawTokenKind::Other, std::string(1, c)});
        i++;
    }

    return tokens;
}

bool isBinaryOperator(const std::string& op) {
    return (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" ||
            op == "=" || op == "==" || op == "!=" || op == "===" || op == "!==" ||
            op == "<" || op == "<=" || op == ">" || op == ">=" ||
            op == "&&" || op == "||" || op == "&" || op == "|" || op == "^" ||
            op == "<<" || op == ">>" || op == "=>" ||
            op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=");
}

bool isControlKeyword(const std::string& kw) {
    return (kw == "if" || kw == "while" || kw == "foreach" || kw == "for" ||
            kw == "catch" || kw == "switch");
}

bool isKeyword(const std::string& kw) {
    return (kw == "fn" || kw == "function" || kw == "let" || kw == "mut" ||
            kw == "return" || kw == "if" || kw == "else" || kw == "while" ||
            kw == "foreach" || kw == "break" || kw == "continue" || kw == "struct" ||
            kw == "enum" || kw == "import" || kw == "class" || kw == "interface" ||
            kw == "implements" || kw == "unsafe" || kw == "extern" || kw == "as" ||
            kw == "echo" || kw == "dynamic" || kw == "new" || kw == "public" ||
            kw == "private" || kw == "protected" || kw == "try" || kw == "catch" ||
            kw == "finally" || kw == "throw" || kw == "async" || kw == "await");
}

} // namespace

std::string Formatter::makeIndent(int level) const {
    if (level <= 0) return "";
    if (m_options.useTabs) {
        return std::string(level, '\t');
    }
    return std::string(level * m_options.indentSpaces, ' ');
}

std::string Formatter::formatTokensAndWhitespace(std::string_view source) const {
    auto tokens = tokenizeForFormat(source);
    if (tokens.empty()) return "";

    std::string result;
    result.reserve(source.size());

    int indentLevel = 0;
    int consecutiveEmptyLines = 0;
    bool atStartOfLine = true;
    RawToken lastNonWsToken{RawTokenKind::Other, ""};

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& tok = tokens[i];

        if (tok.kind == RawTokenKind::Whitespace) {
            // Ignore original whitespace; we re-synthesize clean spacing
            continue;
        }

        if (tok.kind == RawTokenKind::Newline) {
            if (atStartOfLine) {
                // Empty line
                if (consecutiveEmptyLines < m_options.maxEmptyLines && !result.empty()) {
                    result += '\n';
                    consecutiveEmptyLines++;
                }
            } else {
                result += '\n';
                atStartOfLine = true;
                consecutiveEmptyLines = 0;
            }
            continue;
        }

        // Lookahead to see if next non-whitespace token is closing brace/bracket
        // To dedent the current line BEFORE printing it if the line starts with closing brace
        if (atStartOfLine) {
            if (tok.text == "}" || tok.text == "]" || tok.text == ")") {
                if (indentLevel > 0) {
                    indentLevel--;
                }
            } else if (tok.kind == RawTokenKind::Identifier && tok.text == "else") {
                // else or catch starts on its own line after a closing brace
            }

            // Don't indent <?php tag
            if (tok.kind != RawTokenKind::PhpOpenTag) {
                result += makeIndent(indentLevel);
            }
            atStartOfLine = false;
            consecutiveEmptyLines = 0;
        }

        // Adjust spacing before this token
        if (!result.empty() && result.back() != '\n' && result.back() != ' ' && result.back() != '\t') {
            bool needsSpaceBefore = false;

            if (tok.kind == RawTokenKind::CommentLine || tok.kind == RawTokenKind::CommentBlock) {
                needsSpaceBefore = true;
            } else if (tok.text == "{") {
                // e.g. fn test() { or if (c) {
                needsSpaceBefore = true;
            } else if (tok.text == "->") {
                needsSpaceBefore = false; // $obj->member
            } else if (lastNonWsToken.text == "->") {
                needsSpaceBefore = false;
            } else if (tok.text == "::") {
                needsSpaceBefore = false; // Math::sqrt
            } else if (lastNonWsToken.text == "::") {
                needsSpaceBefore = false;
            } else if (tok.text == "..") {
                needsSpaceBefore = false; // slice 0..10
            } else if (lastNonWsToken.text == "..") {
                needsSpaceBefore = false;
            } else if (tok.text == "(") {
                // Space before ( if control keyword: if (, while (, foreach (
                if (lastNonWsToken.kind == RawTokenKind::Identifier && isControlKeyword(lastNonWsToken.text)) {
                    needsSpaceBefore = true;
                } else {
                    needsSpaceBefore = false;
                }
            } else if (tok.text == ")") {
                needsSpaceBefore = false;
            } else if (tok.text == "[") {
                needsSpaceBefore = false;
            } else if (tok.text == "]") {
                needsSpaceBefore = false;
            } else if (tok.text == ",") {
                needsSpaceBefore = false;
            } else if (tok.text == ";") {
                needsSpaceBefore = false;
            } else if (tok.text == ":") {
                // Colon in type annotation or ternary
                if (lastNonWsToken.kind == RawTokenKind::Identifier || lastNonWsToken.text == ")") {
                    needsSpaceBefore = false; // param: i32 or fn f(): i32
                } else {
                    needsSpaceBefore = true; // ternary ? :
                }
            } else if (tok.text == "?") {
                needsSpaceBefore = true;
            } else if (isBinaryOperator(tok.text)) {
                // Check if unary minus/plus or pointer
                if ((tok.text == "-" || tok.text == "+") &&
                    (lastNonWsToken.kind == RawTokenKind::Operator ||
                     lastNonWsToken.text == "(" || lastNonWsToken.text == "[" ||
                     lastNonWsToken.text == "=" || lastNonWsToken.text == "," ||
                     lastNonWsToken.text == "return" || lastNonWsToken.text.empty())) {
                    needsSpaceBefore = (lastNonWsToken.text == "return" || lastNonWsToken.text == ",");
                } else if ((tok.text == "*" || tok.text == "&") &&
                           (lastNonWsToken.text == ":" || lastNonWsToken.text == "mut" ||
                            lastNonWsToken.text == "let" || lastNonWsToken.text == "as" ||
                            lastNonWsToken.text == "(" || lastNonWsToken.text == ",")) {
                    needsSpaceBefore = (lastNonWsToken.text != "(");
                } else {
                    needsSpaceBefore = true;
                }
            } else if (isBinaryOperator(lastNonWsToken.text)) {
                needsSpaceBefore = true;
            } else if (lastNonWsToken.text == ",") {
                needsSpaceBefore = true;
            } else if (lastNonWsToken.text == ":") {
                needsSpaceBefore = true;
            } else if (lastNonWsToken.text == ";") {
                needsSpaceBefore = true;
            } else if (lastNonWsToken.kind == RawTokenKind::Identifier && isKeyword(lastNonWsToken.text)) {
                needsSpaceBefore = true;
            } else if (lastNonWsToken.kind == RawTokenKind::Identifier && tok.kind == RawTokenKind::Identifier) {
                needsSpaceBefore = true;
            } else if (lastNonWsToken.kind == RawTokenKind::Identifier && tok.kind == RawTokenKind::Number) {
                needsSpaceBefore = true;
            } else if (lastNonWsToken.kind == RawTokenKind::Number && tok.kind == RawTokenKind::Identifier) {
                needsSpaceBefore = true;
            }

            if (needsSpaceBefore) {
                result += ' ';
            }
        }

        // Append token text
        result += tok.text;

        // Increase indent level AFTER opening brace/bracket
        if (tok.text == "{" || tok.text == "[") {
            indentLevel++;
        }

        lastNonWsToken = tok;
    }

    // Clean trailing whitespace per line if enabled
    if (m_options.trimTrailingWhitespace) {
        std::istringstream stream(result);
        std::string line;
        std::string cleaned;
        bool first = true;
        while (std::getline(stream, line)) {
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
                line.pop_back();
            }
            if (!first) cleaned += '\n';
            cleaned += line;
            first = false;
        }
        result = std::move(cleaned);
    }

    // Ensure final newline
    if (m_options.insertFinalNewline) {
        if (result.empty() || result.back() != '\n') {
            result += '\n';
        }
    }

    return result;
}

FormatResult Formatter::format(std::string_view source) const {
    FormatResult res;
    try {
        std::string formatted = formatTokensAndWhitespace(source);
        res.success = true;
        res.changed = (formatted != source);
        res.formattedCode = std::move(formatted);

        // Count lines
        int count = 0;
        for (char c : res.formattedCode) {
            if (c == '\n') count++;
        }
        res.linesFormatted = count;
    } catch (const std::exception& e) {
        res.success = false;
        res.errorMessage = e.what();
    }
    return res;
}

bool Formatter::isFormatted(std::string_view source) const {
    auto res = format(source);
    return res.success && !res.changed;
}

FormatResult Formatter::formatFile(const std::filesystem::path& path, bool inPlace) const {
    FormatResult res;
    std::ifstream file(path);
    if (!file.is_open()) {
        res.success = false;
        res.errorMessage = "Could not open file: " + path.string();
        return res;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    res = format(content);
    if (res.success && res.changed && inPlace) {
        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open()) {
            res.success = false;
            res.errorMessage = "Could not write to file: " + path.string();
            return res;
        }
        out << res.formattedCode;
        out.close();
    }

    return res;
}

bool Formatter::isFileFormatted(const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return isFormatted(content);
}

} // namespace sph

