#include "common/test_framework.hpp"
#include "compiler/source/SourceLocation.hpp"
#include "compiler/source/SourceManager.hpp"

TEST_CASE("SourceLocation and SourceSpan basics") {
    sph::SourceLocation loc1{1, 10, 2, 5};
    sph::SourceLocation loc2{1, 15, 2, 10};

    CHECK(loc1.isValid());
    CHECK(loc2.isValid());
    CHECK(loc1 < loc2);
    CHECK_NE(loc1, loc2);

    sph::SourceSpan span(loc1, loc2);
    CHECK(span.isValid());
    CHECK_EQ(span.start.byteOffset, 10u);
    CHECK_EQ(span.end.byteOffset, 15u);
}

TEST_CASE("SourceManager line and column calculation") {
    sph::SourceManager sm;
    std::string code = "line 1\nline 2 is longer\n\nline 4";
    uint32_t fileId = sm.addSource("test.sph", code);

    const sph::SourceFile* file = sm.getFile(fileId);
    CHECK(file != nullptr);
    CHECK_EQ(file->getFilename(), "test.sph");
    CHECK_EQ(file->getLineCount(), 4u);

    // Offset 0 is "l" on line 1, col 1
    auto loc0 = sm.getLocation(fileId, 0);
    CHECK_EQ(loc0.line, 1u);
    CHECK_EQ(loc0.column, 1u);

    // Offset 7 is "l" on line 2, col 1
    auto loc7 = sm.getLocation(fileId, 7);
    CHECK_EQ(loc7.line, 2u);
    CHECK_EQ(loc7.column, 1u);

    // Offset 12 is "2" on line 2, col 6
    auto loc12 = sm.getLocation(fileId, 12);
    CHECK_EQ(loc12.line, 2u);
    CHECK_EQ(loc12.column, 6u);

    // Check line content extraction
    CHECK_EQ(sm.getLineContent(fileId, 1), "line 1");
    CHECK_EQ(sm.getLineContent(fileId, 2), "line 2 is longer");
    CHECK_EQ(sm.getLineContent(fileId, 3), "");
    CHECK_EQ(sm.getLineContent(fileId, 4), "line 4");
}

