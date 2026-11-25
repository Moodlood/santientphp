cmake_minimum_required(VERSION 3.20)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SPH_PROJECT_VERSION LIMIT_COUNT 1)
string(STRIP "${SPH_PROJECT_VERSION}" SPH_PROJECT_VERSION)
project(SantientPHP
    VERSION ${SPH_PROJECT_VERSION}
    DESCRIPTION "SantientPHP Compiler"
    LANGUAGES C CXX
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/compiler/Version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/compiler/Version.hpp"
    @ONLY
)
set(SPH_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/sphc.ico")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/sphc/sphc.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/sphc.rc"
    @ONLY
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binaries to a standard directory layout
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(SPH_SANITIZE "" CACHE STRING "Enable sanitizers: address (ASan+UBSan), thread (TSan)")

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>)
    if(SPH_SANITIZE STREQUAL "address")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -O1)
        add_link_options(-fsanitize=address,undefined)
        add_compile_definitions(SPH_SANITIZE_LINK_FLAGS="-fsanitize=address,undefined")
    elseif(SPH_SANITIZE STREQUAL "thread")
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -O1)
        add_link_options(-fsanitize=thread)
        add_compile_definitions(SPH_SANITIZE_LINK_FLAGS="-fsanitize=thread")
    endif()
endif()

find_package(Threads)

# ============================================================================
# Modular SPH Runtime Libraries (Milestone 2)
# ============================================================================

# 1. Core Native Runtime (libsph_core.a: memory, rc, arena, strings, vec, io, simd)
add_library(sph_runtime_core STATIC
    runtime/core/core.h
    runtime/core/core.c
)
target_include_directories(sph_runtime_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
set_target_properties(sph_runtime_core PROPERTIES OUTPUT_NAME "sph_core")

# 2. Async & Concurrency Runtime (libsph_async.a: atomics, threads, fibers, promises)
add_library(sph_runtime_async STATIC
    runtime/async/async.h
    runtime/async/async.c
)
target_include_directories(sph_runtime_async PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
set_target_properties(sph_runtime_async PROPERTIES OUTPUT_NAME "sph_async")

# 3. PHP Compatibility Runtime (libsph_compat.a: dynamic values, arrays, pdo, curl)
add_library(sph_runtime_compat STATIC
    runtime/compat/compat.h
    runtime/compat/compat.c
)
target_include_directories(sph_runtime_compat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
set_target_properties(sph_runtime_compat PROPERTIES OUTPUT_NAME "sph_compat")

# 3b. Networking & HTTP Runtime (libsph_net.a: net, websocket, epoll)
add_library(sph_runtime_net STATIC
    runtime/net/net.c
    runtime/net/websocket.h
    runtime/net/websocket.c
    runtime/net/epoll_engine.h
    runtime/net/epoll_engine.c
)
target_include_directories(sph_runtime_net PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
set_target_properties(sph_runtime_net PROPERTIES OUTPUT_NAME "sph_net")

# 4. Aggregate SPH Runtime Library (libsph_runtime.a)
add_library(sph_runtime STATIC
    runtime/core/runtime.h
    runtime/core/runtime.c
)
target_include_directories(sph_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
set_target_properties(sph_runtime PROPERTIES OUTPUT_NAME "sph_runtime")

# 5. Native GUI Runtime (libsph_gui.a: Win32 controls, X11/GLX backend, SIMD canvas, WebView bridge)
add_library(sph_runtime_gui STATIC
    runtime/gui/gui.h
    runtime/gui/gui.c
)
target_include_directories(sph_runtime_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
set_target_properties(sph_runtime_gui PROPERTIES OUTPUT_NAME "sph_gui")

if(UNIX AND NOT APPLE)
    find_package(X11 QUIET)
    find_package(OpenGL QUIET)
    if(X11_FOUND)
        target_compile_definitions(sph_runtime_gui PUBLIC SPH_HAS_X11=1)
        target_include_directories(sph_runtime_gui PUBLIC ${X11_INCLUDE_DIR})
        target_link_libraries(sph_runtime_gui PUBLIC ${X11_LIBRARIES})
    endif()
    if(OPENGL_FOUND)
        target_compile_definitions(sph_runtime_gui PUBLIC SPH_HAS_GLX=1)
        target_include_directories(sph_runtime_gui PUBLIC ${OPENGL_INCLUDE_DIR})
        target_link_libraries(sph_runtime_gui PUBLIC ${OPENGL_LIBRARIES})
    endif()
endif()

# Enable section-level dead-code stripping & fast runtime execution for all runtime archives
if(NOT MSVC)
    foreach(_rt_target sph_runtime_core sph_runtime_async sph_runtime_compat sph_runtime_net sph_runtime sph_runtime_gui)
        target_compile_options(${_rt_target} PRIVATE
            -ffunction-sections
            -fdata-sections
            $<$<NOT:$<CONFIG:Debug>>:-O2>
        )
    endforeach()
endif()

# Core compiler static library
add_library(sphc_core STATIC
    compiler/source/SourceLocation.hpp
    compiler/source/SourceManager.hpp
    compiler/source/SourceManager.cpp
    compiler/diagnostics/Diagnostic.hpp
    compiler/diagnostics/Diagnostic.cpp
    compiler/diagnostics/DiagnosticEmitter.hpp
    compiler/diagnostics/DiagnosticEmitter.cpp
    compiler/lexer/Token.hpp
    compiler/lexer/Token.cpp
    compiler/lexer/Lexer.hpp
    compiler/lexer/Lexer.cpp
    compiler/ast/ASTNode.hpp
    compiler/ast/ASTVisitor.hpp
    compiler/ast/ExprAST.hpp
    compiler/ast/StmtAST.hpp
    compiler/ast/DeclAST.hpp
    compiler/ast/ASTPrinter.hpp
    compiler/ast/ASTPrinter.cpp
    compiler/parser/Parser.hpp
    compiler/parser/Parser.cpp
    compiler/types/Type.hpp
    compiler/types/Type.cpp
    compiler/manifest/Manifest.hpp
    compiler/manifest/Manifest.cpp
    compiler/semantic/Symbol.hpp
    compiler/semantic/Scope.hpp
    compiler/semantic/Scope.cpp
    compiler/semantic/SemanticAnalyzer.hpp
    compiler/semantic/SemanticAnalyzer.cpp
    compiler/semantic/BorrowChecker.hpp
    compiler/semantic/BorrowChecker.cpp
    compiler/ir/IRValue.hpp
    compiler/ir/IRValue.cpp
    compiler/ir/IRInstruction.hpp
    compiler/ir/IRInstruction.cpp
    compiler/ir/IRBasicBlock.hpp
    compiler/ir/IRBasicBlock.cpp
    compiler/ir/IRFunction.hpp
    compiler/ir/IRFunction.cpp
    compiler/ir/IRModule.hpp
    compiler/ir/IRModule.cpp
    compiler/ir/IRPrinter.hpp
    compiler/ir/IRPrinter.cpp
    compiler/ir/IRVerifier.hpp
    compiler/ir/IRVerifier.cpp
    compiler/ir/IRGenerator.hpp
    compiler/ir/IRGenerator.cpp
    compiler/codegen/x86_64/LinearScanAllocator.hpp
    compiler/codegen/x86_64/LinearScanAllocator.cpp
    compiler/codegen/x86_64/X86_64CodeGen.hpp
    compiler/codegen/x86_64/X86_64CodeGen.cpp
    compiler/codegen/llvm/LLVMEmitter.hpp
    compiler/codegen/llvm/LLVMEmitter.cpp
    compiler/codegen/Linker.hpp
    compiler/codegen/Linker.cpp
    compiler/optimizer/Pass.hpp
    compiler/optimizer/Dominance.hpp
    compiler/optimizer/Dominance.cpp
    compiler/optimizer/Mem2RegPass.hpp
    compiler/optimizer/Mem2RegPass.cpp
    compiler/optimizer/CriticalEdgeSplittingPass.hpp
    compiler/optimizer/CriticalEdgeSplittingPass.cpp
    compiler/optimizer/EscapeAnalysisPass.hpp
    compiler/optimizer/EscapeAnalysisPass.cpp
    compiler/optimizer/TailCallPass.hpp
    compiler/optimizer/TailCallPass.cpp
    compiler/optimizer/ConstantFoldingPass.hpp
    compiler/optimizer/ConstantFoldingPass.cpp
    compiler/optimizer/AlgebraicSimplificationPass.hpp
    compiler/optimizer/AlgebraicSimplificationPass.cpp
    compiler/optimizer/DeadCodeEliminationPass.hpp
    compiler/optimizer/DeadCodeEliminationPass.cpp
    compiler/optimizer/DeadStoreEliminationPass.hpp
    compiler/optimizer/DeadStoreEliminationPass.cpp
    compiler/optimizer/StrengthReductionPass.hpp
    compiler/optimizer/StrengthReductionPass.cpp
    compiler/optimizer/LoopInvariantCodeMotionPass.hpp
    compiler/optimizer/LoopInvariantCodeMotionPass.cpp
    compiler/optimizer/LoopUnrollPass.hpp
    compiler/optimizer/LoopUnrollPass.cpp
    compiler/optimizer/CommonSubexpressionEliminationPass.hpp
    compiler/optimizer/CommonSubexpressionEliminationPass.cpp
    compiler/optimizer/ControlFlowOptimizationPass.hpp
    compiler/optimizer/ControlFlowOptimizationPass.cpp
    compiler/optimizer/FunctionInliningPass.hpp
    compiler/optimizer/FunctionInliningPass.cpp
    compiler/optimizer/StoreToLoadForwardingPass.hpp
    compiler/optimizer/StoreToLoadForwardingPass.cpp
    compiler/optimizer/PassManager.hpp
    compiler/optimizer/PassManager.cpp
    compiler/target/Target.hpp
    compiler/target/Target.cpp
    compiler/formatter/Formatter.hpp
    compiler/formatter/Formatter.cpp
    compiler/docgen/DocComment.hpp
    compiler/docgen/DocComment.cpp
    compiler/docgen/DocModel.hpp
    compiler/docgen/DocExtractor.hpp
    compiler/docgen/DocExtractor.cpp
    compiler/docgen/DocGenerator.hpp
    compiler/docgen/DocGenerator.cpp
    compiler/package/PackageManager.hpp
    compiler/package/PackageManager.cpp
    compiler/package/SemVer.hpp
    compiler/package/SemVer.cpp
    compiler/package/Lockfile.hpp
    compiler/package/Lockfile.cpp
    compiler/lsp/Json.hpp
    compiler/lsp/LspServer.hpp
    compiler/lsp/LspServer.cpp
    compiler/doctor/Doctor.hpp
    compiler/doctor/Doctor.cpp
)

target_include_directories(sphc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Compiler CLI driver: sphc
set(SPHC_EXTRA_SOURCES "")
if(WIN32)
    set(SPHC_EXTRA_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/generated/sphc.rc")
endif()

add_executable(sphc
    tools/sphc/main.cpp
    ${SPHC_EXTRA_SOURCES}
)

target_link_libraries(sphc PRIVATE sphc_core)
if(UNIX AND NOT APPLE)
    target_link_libraries(sphc PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()
add_dependencies(sphc sph_runtime sph_runtime_core sph_runtime_async sph_runtime_compat sph_runtime_net sph_runtime_gui)

# Differential IR & Codegen Fuzzer
add_executable(sph_diff_fuzz
    tools/fuzz/fuzz_ir_diff.cpp
)
target_link_libraries(sph_diff_fuzz PRIVATE sphc_core)
if(UNIX AND NOT APPLE)
    target_link_libraries(sph_diff_fuzz PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()
add_dependencies(sph_diff_fuzz sph_runtime_core)

# Test Suite
enable_testing()

add_executable(sph_tests
    tests/test_main.cpp
    tests/source/test_source_manager.cpp
    tests/diagnostics/test_diagnostics.cpp
    tests/lexer/test_lexer.cpp
    tests/parser/test_parser.cpp
    tests/semantic/test_semantic.cpp
    tests/semantic/test_option_result.cpp
    tests/semantic/test_pattern_matching.cpp
    tests/semantic/test_negative_suite.cpp
    tests/diagnostics/test_diagnostic_codes.cpp
    tests/manifest/test_manifest.cpp
    tests/ir/test_ir.cpp
    tests/codegen/test_codegen.cpp
    tests/compat/test_php_compat.cpp
    tests/optimizer/test_optimizer.cpp
    tests/debug/test_debugger.cpp
    tests/target/test_target.cpp
    tests/formatter/test_formatter.cpp
    tests/docgen/test_docgen.cpp
    tests/package/test_package.cpp
    tests/lsp/test_lsp.cpp
    tests/doctor/test_doctor.cpp
    tests/stdlib/test_stdlib.cpp
    tests/gui/test_gui.cpp
    tests/lexer/test_lexer_exhaustive.cpp
    tests/parser/test_parser_exhaustive.cpp
    tests/semantic/test_type_system_exhaustive.cpp
    tests/optimizer/test_optimizer_exhaustive.cpp
    tests/stdlib/test_stdlib_exhaustive.cpp
    tests/codegen/test_codegen_exhaustive.cpp
    tests/optimizer/test_dominance_exhaustive.cpp
    tests/optimizer/test_mem2reg_exhaustive.cpp
    tests/codegen/test_lsra_exhaustive.cpp
    tests/optimizer/test_escape_analysis_exhaustive.cpp
    tests/runtime/test_slab_exhaustive.cpp
    tests/codegen/test_tco_exhaustive.cpp
    tests/semantic/test_borrow_checker_nll_exhaustive.cpp
    tests/runtime/test_net_exhaustive.cpp
    tests/codegen/test_v02_stress_integration.cpp
    tests/ir/test_sphir_verifier_exhaustive.cpp
    tests/codegen/test_parallel_copy_exhaustive.cpp
    tests/runtime/test_memory_safety_exhaustive.cpp
    tests/codegen/test_backend_parity.cpp
    tests/runtime/test_concurrency_stress_exhaustive.cpp
    tests/optimizer/test_property_based_exhaustive.cpp
)

target_link_libraries(sph_tests PRIVATE sphc_core sph_runtime sph_runtime_gui)
if(WIN32)
    target_link_libraries(sph_tests PRIVATE comctl32 gdi32 user32 opengl32 ole32 winmm ws2_32 winhttp)
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(sph_tests PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    if(X11_FOUND)
        target_link_libraries(sph_tests PRIVATE ${X11_LIBRARIES})
    endif()
    if(OPENGL_FOUND)
        target_link_libraries(sph_tests PRIVATE ${OPENGL_LIBRARIES})
    endif()
endif()
target_include_directories(sph_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_dependencies(sph_tests sph_runtime sph_runtime_gui)

add_test(NAME UnitTests COMMAND sph_tests)
set_tests_properties(UnitTests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# ==============================================================================
# Installation Targets
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS sphc
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS sph_runtime sph_runtime_core sph_runtime_async sph_runtime_compat sph_runtime_net sph_runtime_gui sphc_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY stdlib/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/santientphp/stdlib
    FILES_MATCHING PATTERN "*.sph"
)

install(DIRECTORY runtime/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/santientphp/runtime
    FILES_MATCHING PATTERN "*.h" PATTERN "*.c"
)

install(DIRECTORY runtime/core/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/santientphp
    FILES_MATCHING PATTERN "*.h"
)

install(FILES sph.ini sph.ini-development sph.ini-production
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/santientphp
)

