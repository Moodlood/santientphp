#include "Diagnostic.hpp"

namespace sph {

const char* diagnosticSeverityToString(DiagnosticSeverity severity) noexcept {
    switch (severity) {
        case DiagnosticSeverity::Error:   return "error";
        case DiagnosticSeverity::Warning: return "warning";
        case DiagnosticSeverity::Note:    return "note";
    }
    return "unknown";
}

} // namespace sph

