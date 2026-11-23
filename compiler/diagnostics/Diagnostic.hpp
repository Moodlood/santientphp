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

struct DiagnosticSpan {
    SourceSpan span;
    std::string label;
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    SourceSpan span;
    std::string message;
    std::string code;
    std::string hint;
    std::vector<DiagnosticSpan> relatedSpans;

    Diagnostic() = default;
    Diagnostic(DiagnosticSeverity sev, SourceSpan sp, std::string msg, std::string c = "", std::string h = "")
        : severity(sev), span(sp), message(std::move(msg)), code(std::move(c)), hint(std::move(h)) {}

    void addRelatedSpan(SourceSpan sp, std::string label) {
        relatedSpans.push_back(DiagnosticSpan{sp, std::move(label)});
    }

    static Diagnostic makeError(SourceSpan span, std::string message, std::string code = "", std::string hint = "") {
        return Diagnostic(DiagnosticSeverity::Error, span, std::move(message), std::move(code), std::move(hint));
    }

    static Diagnostic makeWarning(SourceSpan span, std::string message, std::string code = "", std::string hint = "") {
        return Diagnostic(DiagnosticSeverity::Warning, span, std::move(message), std::move(code), std::move(hint));
    }

    static Diagnostic makeNote(SourceSpan span, std::string message, std::string code = "", std::string hint = "") {
        return Diagnostic(DiagnosticSeverity::Note, span, std::move(message), std::move(code), std::move(hint));
    }
};

} // namespace sph

