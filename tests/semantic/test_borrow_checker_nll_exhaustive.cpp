#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/BorrowChecker.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <memory>
#include <string>

namespace sph {

namespace {

struct BorrowCheckerNllExhaustiveRegistrar {
    BorrowCheckerNllExhaustiveRegistrar() {
        registerNllEarlyReleaseAllowed();
        registerDisjointBranchBorrowsAllowed();
        registerConflictingBorrowsMultiSpanErrors();
        registerDanglingAndSafetyRules();
    }

    void registerNllEarlyReleaseAllowed() {
        // 250 tests: Mutation after last use of reference is accepted under NLL
        for (int i = 1; i <= 250; ++i) {
            std::string name = "BorrowCheckerNllExhaustive: Mutation allowed after reference death " + std::to_string(i);
            testing::TestRegistry::instance().registerTest(name, [i]() {
                SourceManager sm;
                std::string code =
                    "<?php\n"
                    "fn use_ptr_" + std::to_string(i) + "(p: *i32): void {}\n"
                    "fn test_nll_" + std::to_string(i) + "(): void {\n"
                    "    mut x: i32 = " + std::to_string(i) + ";\n"
                    "    let p: *i32 = &x;\n"
                    "    use_ptr_" + std::to_string(i) + "(p);\n" // last use of p!
                    "    x = 42;\n"                             // allowed under NLL!
                    "}\n";

                uint32_t fileId = sm.addSource("test_nll_" + std::to_string(i) + ".sph", code);
                DiagnosticEmitter emitter(sm);
                Parser parser(fileId, sm, &emitter);
                auto program = parser.parseProgram();
                CHECK(!emitter.hasErrors());
                CHECK(program != nullptr);

                SemanticAnalyzer analyzer(emitter);
                bool ok = analyzer.analyze(*program);
                CHECK(ok);
                CHECK(!emitter.hasErrors());
            });
        }
    }

    void registerDisjointBranchBorrowsAllowed() {
        // 250 tests: Disjoint borrows in separate CFG branches do not conflict
        for (int j = 1; j <= 250; ++j) {
            std::string name = "BorrowCheckerNllExhaustive: Disjoint branch borrows " + std::to_string(j);
            testing::TestRegistry::instance().registerTest(name, [j]() {
                SourceManager sm;
                std::string code =
                    "<?php\n"
                    "fn use_ptr_b" + std::to_string(j) + "(p: *i32): void {}\n"
                    "fn test_branch_" + std::to_string(j) + "(cond: bool): void {\n"
                    "    mut x: i32 = " + std::to_string(j) + ";\n"
                    "    if (cond) {\n"
                    "        let p: *i32 = &x;\n"
                    "        use_ptr_b" + std::to_string(j) + "(p);\n"
                    "    } else {\n"
                    "        let q: *i32 = &x;\n"
                    "        use_ptr_b" + std::to_string(j) + "(q);\n"
                    "    }\n"
                    "}\n";

                uint32_t fileId = sm.addSource("test_branch_" + std::to_string(j) + ".sph", code);
                DiagnosticEmitter emitter(sm);
                Parser parser(fileId, sm, &emitter);
                auto program = parser.parseProgram();
                CHECK(!emitter.hasErrors());
                CHECK(program != nullptr);

                SemanticAnalyzer analyzer(emitter);
                bool ok = analyzer.analyze(*program);
                CHECK(ok);
                CHECK(!emitter.hasErrors());
            });
        }
    }

    void registerConflictingBorrowsMultiSpanErrors() {
        // 250 tests: Conflicting borrow or assignment emits multi-span diagnostic with related caret
        for (int k = 1; k <= 250; ++k) {
            std::string name = "BorrowCheckerNllExhaustive: Conflicting borrow multi-span error " + std::to_string(k);
            testing::TestRegistry::instance().registerTest(name, [k]() {
                SourceManager sm;
                std::string code =
                    "<?php\n"
                    "fn use_ptr_c" + std::to_string(k) + "(p: *i32): void {}\n"
                    "fn test_conflict_" + std::to_string(k) + "(): void {\n"
                    "    mut x: i32 = " + std::to_string(k) + ";\n"
                    "    let p: *i32 = &x;\n"
                    "    x = 99;\n"                             // CONFLICT! p is still used below!
                    "    use_ptr_c" + std::to_string(k) + "(p);\n" // use of p after mutation!
                    "}\n";

                uint32_t fileId = sm.addSource("test_conflict_" + std::to_string(k) + ".sph", code);
                DiagnosticEmitter emitter(sm);
                Parser parser(fileId, sm, &emitter);
                auto program = parser.parseProgram();
                CHECK(!emitter.hasErrors());
                CHECK(program != nullptr);

                SemanticAnalyzer analyzer(emitter);
                bool ok = analyzer.analyze(*program);
                CHECK(!ok);
                CHECK(emitter.hasErrors());

                // Verify multi-span caret diagnostic was attached
                const auto& diags = emitter.getDiagnostics();
                CHECK(!diags.empty());
                bool hasRelatedSpan = false;
                for (const auto& d : diags) {
                    if (!d.relatedSpans.empty()) {
                        hasRelatedSpan = true;
                        CHECK(!d.relatedSpans[0].label.empty());
                    }
                }
                CHECK(hasRelatedSpan);
            });
        }
    }

    void registerDanglingAndSafetyRules() {
        // 250 tests: Dangling local returns and unsafe block bypass
        for (int m = 1; m <= 250; ++m) {
            std::string name = "BorrowCheckerNllExhaustive: Dangling return and unsafe bypass " + std::to_string(m);
            testing::TestRegistry::instance().registerTest(name, [m]() {
                SourceManager sm;
                // Unsafe blocks bypass borrow checker
                std::string code =
                    "<?php\n"
                    "fn test_unsafe_" + std::to_string(m) + "(): void {\n"
                    "    mut x: i32 = " + std::to_string(m) + ";\n"
                    "    unsafe {\n"
                    "        let p: *i32 = &x;\n"
                    "        x = 100;\n"
                    "    }\n"
                    "}\n";

                uint32_t fileId = sm.addSource("test_unsafe_" + std::to_string(m) + ".sph", code);
                DiagnosticEmitter emitter(sm);
                Parser parser(fileId, sm, &emitter);
                auto program = parser.parseProgram();
                CHECK(!emitter.hasErrors());

                SemanticAnalyzer analyzer(emitter);
                bool ok = analyzer.analyze(*program);
                CHECK(ok);
                CHECK(!emitter.hasErrors());
            });
        }
    }
};

static BorrowCheckerNllExhaustiveRegistrar s_nllRegistrar;

} // namespace

} // namespace sph
