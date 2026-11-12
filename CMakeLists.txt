cmake_minimum_required(VERSION 3.20)
project(SantientPHP
    VERSION 0.1.0
    DESCRIPTION "SantientPHP Compiler"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binaries to a standard directory layout
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Minimal SPH Modular Runtime Library
add_library(sph_runtime STATIC
    runtime/core/runtime.h
    runtime/core/runtime.c
)

target_include_directories(sph_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(sph_runtime PROPERTIES OUTPUT_NAME "sph_runtime")

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
    compiler/codegen/x86_64/X86_64CodeGen.hpp
    compiler/codegen/x86_64/X86_64CodeGen.cpp
    compiler/codegen/llvm/LLVMEmitter.hpp
    compiler/codegen/llvm/LLVMEmitter.cpp
    compiler/codegen/Linker.hpp
    compiler/codegen/Linker.cpp
    compiler/optimizer/Pass.hpp
    compiler/optimizer/ConstantFoldingPass.hpp
    compiler/optimizer/ConstantFoldingPass.cpp
    compiler/optimizer/AlgebraicSimplificationPass.hpp
    compiler/optimizer/AlgebraicSimplificationPass.cpp
    compiler/optimizer/DeadCodeEliminationPass.hpp
    compiler/optimizer/DeadCodeEliminationPass.cpp
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
    compiler/lsp/Json.hpp
    compiler/lsp/LspServer.hpp
    compiler/lsp/LspServer.cpp
    compiler/doctor/Doctor.hpp
    compiler/doctor/Doctor.cpp
)

target_include_directories(sphc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler CLI driver: sphc
add_executable(sphc
    tools/sphc/main.cpp
)

target_link_libraries(sphc PRIVATE sphc_core)
add_dependencies(sphc sph_runtime)

# Test Suite
enable_testing()

add_executable(sph_tests
    tests/test_main.cpp
    tests/source/test_source_manager.cpp
    tests/diagnostics/test_diagnostics.cpp
    tests/lexer/test_lexer.cpp
    tests/parser/test_parser.cpp
    tests/semantic/test_semantic.cpp
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
)

target_link_libraries(sph_tests PRIVATE sphc_core)
target_include_directories(sph_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_dependencies(sph_tests sph_runtime)

add_test(NAME UnitTests COMMAND sph_tests)
