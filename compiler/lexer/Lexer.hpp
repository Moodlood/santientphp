#pragma once

#include "Token.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/source/SourceManager.hpp"
#include <string_view>
#include <vector>

namespace sph {

class Lexer {
public:
    Lexer(uint32_t fileId, const SourceManager& sourceManager, DiagnosticEmitter* diagnostics = nullptr);

    Token nextToken();
    std::vector<Token> tokenizeAll();

private:
    [[nodiscard]] bool isAtEnd() const noexcept { return m_cursor >= m_content.size(); }
    [[nodiscard]] char peek() const noexcept;
    [[nodiscard]] char peekNext() const noexcept;
    char advance() noexcept;
    bool match(char expected) noexcept;

    void skipWhitespaceAndComments();
    Token scanIdentifierOrKeyword(uint32_t startOffset);
    Token scanNumber(uint32_t startOffset);
    Token scanString(char quoteChar, uint32_t startOffset);

    [[nodiscard]] SourceLocation makeLocation(uint32_t offset) const noexcept;
    [[nodiscard]] SourceSpan makeSpan(uint32_t startOffset, uint32_t endOffset) const noexcept;

    void error(uint32_t startOffset, uint32_t endOffset, std::string message);

    uint32_t m_fileId;
    const SourceManager& m_sourceManager;
    DiagnosticEmitter* m_diagnostics;
    std::string_view m_content;
    uint32_t m_cursor = 0;
    bool m_seenOpeningTag = false;
};

} // namespace sph

