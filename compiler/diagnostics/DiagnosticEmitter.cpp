#include "DiagnosticEmitter.hpp"
#include <iostream>
#include <sstream>

namespace sph {

DiagnosticEmitter::DiagnosticEmitter(const SourceManager& sourceManager, std::ostream* outStream)
    : m_sourceManager(sourceManager), m_outStream(outStream ? outStream : &std::cerr) {}

void DiagnosticEmitter::emit(const Diagnostic& diagnostic) {
    m_diagnostics.push_back(diagnostic);
    if (diagnostic.severity == DiagnosticSeverity::Error) {
        ++m_errorCount;
    } else if (diagnostic.severity == DiagnosticSeverity::Warning) {
        ++m_warningCount;
    }

    if (m_outStream) {
        *m_outStream << format(diagnostic) << "\n";
    }
}

std::string DiagnosticEmitter::format(const Diagnostic& diagnostic) const {
    std::ostringstream ss;

    const SourceFile* file = m_sourceManager.getFile(diagnostic.span.start.fileId);
    std::string filename = file ? file->getFilename() : "<unknown>";
    uint32_t line = diagnostic.span.start.line;
    uint32_t col = diagnostic.span.start.column;

    ss << filename << ":" << line << ":" << col << ": "
       << diagnosticSeverityToString(diagnostic.severity) << ": "
       << diagnostic.message;

    if (file && line > 0) {
        std::string_view lineContent = file->getLineContent(line);
        if (!lineContent.empty()) {
            ss << "\n    " << lineContent << "\n    ";

            // Add spaces up to column (1-based)
            uint32_t prefixSpaces = (col > 0) ? (col - 1) : 0;
            // Handle tabs in source line cleanly by matching their width
            for (uint32_t i = 0; i < prefixSpaces && i < lineContent.size(); ++i) {
                if (lineContent[i] == '\t') {
                    ss << '\t';
                } else {
                    ss << ' ';
                }
            }

            // Determine caret span length
            uint32_t caretCount = 1;
            if (diagnostic.span.end.line == line && diagnostic.span.end.column >= col) {
                caretCount = (diagnostic.span.end.column - col);
                if (caretCount == 0) caretCount = 1;
            }

            for (uint32_t i = 0; i < caretCount; ++i) {
                ss << '^';
            }
        }
    }

    return ss.str();
}

} // namespace sph

