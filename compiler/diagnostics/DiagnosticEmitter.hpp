#pragma once

#include "Diagnostic.hpp"
#include "compiler/source/SourceManager.hpp"
#include <iosfwd>
#include <string>
#include <vector>

namespace sph {

class DiagnosticEmitter {
public:
    explicit DiagnosticEmitter(const SourceManager& sourceManager, std::ostream* outStream = nullptr);

    void emit(const Diagnostic& diagnostic);

    [[nodiscard]] std::string format(const Diagnostic& diagnostic) const;

    [[nodiscard]] uint32_t getErrorCount() const noexcept { return m_errorCount; }
    [[nodiscard]] uint32_t getWarningCount() const noexcept { return m_warningCount; }
    [[nodiscard]] bool hasErrors() const noexcept { return m_errorCount > 0; }
    [[nodiscard]] const std::vector<Diagnostic>& getDiagnostics() const noexcept { return m_diagnostics; }

    void resetCounts() noexcept {
        m_errorCount = 0;
        m_warningCount = 0;
        m_diagnostics.clear();
    }

private:
    const SourceManager& m_sourceManager;
    std::ostream* m_outStream;
    uint32_t m_errorCount = 0;
    uint32_t m_warningCount = 0;
    std::vector<Diagnostic> m_diagnostics;
};

} // namespace sph

