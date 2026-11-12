#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/source/SourceManager.hpp"
#include <sstream>

TEST_CASE("Diagnostic formatting with caret underline") {
    sph::SourceManager sm;
    std::string code = "<?php\nlet count: i32 = \"bad\";\n";
    uint32_t fileId = sm.addSource("examples/bad.sph", code);

    // "bad" starts at byte 23 (line 2, col 18) and ends at byte 28 (line 2, col 23)
    sph::SourceLocation startLoc = sm.getLocation(fileId, 23);
    sph::SourceLocation endLoc = sm.getLocation(fileId, 28);
    sph::SourceSpan span(startLoc, endLoc);

    std::ostringstream out;
    sph::DiagnosticEmitter emitter(sm, &out);

    auto diag = sph::Diagnostic::makeError(span, "expected i32, found string");
    emitter.emit(diag);

    CHECK(emitter.hasErrors());
    CHECK_EQ(emitter.getErrorCount(), 1u);
    CHECK_EQ(emitter.getWarningCount(), 0u);

    std::string formatted = emitter.format(diag);
    // Should match standard compiler diagnostics format:
    // examples/bad.sph:2:18: error: expected i32, found string
    CHECK(formatted.find("examples/bad.sph:2:18: error: expected i32, found string") != std::string::npos);
    CHECK(formatted.find("let count: i32 = \"bad\";") != std::string::npos);
    CHECK(formatted.find("^^^^^") != std::string::npos);
}

