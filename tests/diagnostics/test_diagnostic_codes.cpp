#include "common/test_framework.hpp"
#include "compiler/diagnostics/Diagnostic.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/source/SourceManager.hpp"
#include <sstream>

TEST_CASE("Diagnostics: Formats error code [E0308] and message") {
    sph::SourceManager sm;
    uint32_t fileId = sm.addSource("diag_test.sph", "let x: i32 = \"text\";\n");
    sph::DiagnosticEmitter emitter(sm);

    sph::SourceLocation start = sm.getLocation(fileId, 13);
    sph::SourceLocation end = sm.getLocation(fileId, 19);
    sph::SourceSpan span(start, end);

    sph::Diagnostic diag = sph::Diagnostic::makeError(span, "mismatched types: expected 'i32', found 'string'", "E0308", "consider parsing the string as an integer");
    std::string formatted = emitter.format(diag);

    CHECK(formatted.find("error[E0308]") != std::string::npos);
    CHECK(formatted.find("mismatched types") != std::string::npos);
    CHECK(formatted.find("^") != std::string::npos);
    CHECK(formatted.find("= help: consider parsing the string as an integer") != std::string::npos);
}

TEST_CASE("Diagnostics: Formats warning without code") {
    sph::SourceManager sm;
    uint32_t fileId = sm.addSource("warn_test.sph", "let unused = 42;\n");
    sph::DiagnosticEmitter emitter(sm);

    sph::SourceLocation start = sm.getLocation(fileId, 4);
    sph::SourceLocation end = sm.getLocation(fileId, 10);
    sph::SourceSpan span(start, end);

    sph::Diagnostic diag = sph::Diagnostic::makeWarning(span, "unused variable 'unused'");
    std::string formatted = emitter.format(diag);

    CHECK(formatted.find("warning: unused variable 'unused'") != std::string::npos);
    CHECK(formatted.find("[") == std::string::npos); // No code bracket
}

TEST_CASE("Diagnostics: Emitter tracks error and warning counts") {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);

    CHECK_EQ(emitter.getErrorCount(), 0u);
    CHECK_EQ(emitter.getWarningCount(), 0u);
    CHECK(!emitter.hasErrors());

    sph::SourceSpan span;
    emitter.emit(sph::Diagnostic::makeError(span, "first error", "E0001"));
    CHECK_EQ(emitter.getErrorCount(), 1u);
    CHECK(emitter.hasErrors());

    emitter.emit(sph::Diagnostic::makeWarning(span, "first warning"));
    CHECK_EQ(emitter.getWarningCount(), 1u);

    emitter.emit(sph::Diagnostic::makeError(span, "second error", "E0002"));
    CHECK_EQ(emitter.getErrorCount(), 2u);

    emitter.resetCounts();
    CHECK_EQ(emitter.getErrorCount(), 0u);
    CHECK_EQ(emitter.getWarningCount(), 0u);
    CHECK(!emitter.hasErrors());
}

TEST_CASE("Diagnostics: Underline pointer spans correct character count") {
    sph::SourceManager sm;
    uint32_t fileId = sm.addSource("span_test.sph", "    return 12345;\n");
    sph::DiagnosticEmitter emitter(sm);

    // Byte offsets 11 to 16 for '12345'
    sph::SourceLocation start = sm.getLocation(fileId, 11);
    sph::SourceLocation end = sm.getLocation(fileId, 16);
    sph::SourceSpan span(start, end);

    sph::Diagnostic diag = sph::Diagnostic::makeError(span, "unexpected literal");
    std::string formatted = emitter.format(diag);

    CHECK(formatted.find("^^^^^") != std::string::npos);
}

