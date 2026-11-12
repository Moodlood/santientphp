#pragma once

#include "compiler/source/SourceLocation.hpp"
#include <string>
#include <vector>

namespace sph {

enum class DiagnosticSeverity {
    Error,
    Warning,
    Note
};

[[nodiscard]] const char* diagnosticSeverityToString(DiagnosticSeverity severity) noexcept;

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    SourceSpan span;
    std::string message;

    Diagnostic() = default;
    Diagnostic(DiagnosticSeverity sev, SourceSpan sp, std::string msg)
        : severity(sev), span(sp), message(std::move(msg)) {}

    static Diagnostic makeError(SourceSpan span, std::string message) {
        return Diagnostic(DiagnosticSeverity::Error, span, std::move(message));
    }

    static Diagnostic makeWarning(SourceSpan span, std::string message) {
        return Diagnostic(DiagnosticSeverity::Warning, span, std::move(message));
    }

    static Diagnostic makeNote(SourceSpan span, std::string message) {
        return Diagnostic(DiagnosticSeverity::Note, span, std::move(message));
    }
};

} // namespace sph

