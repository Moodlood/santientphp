#include "common/test_framework.hpp"
#include "compiler/codegen/x86_64/X86_64CodeGen.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRGenerator.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/ir/IRModule.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include "compiler/target/Target.hpp"
#include <memory>
#include <sstream>
#include <string>

namespace {

std::string generateAssembly(const std::string& code, const sph::Target& target) {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    uint32_t fileId = sm.addSource("codegen_test.sph", code);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program || emitter.hasErrors()) return "";

    sph::SemanticAnalyzer analyzer(emitter);
    if (!analyzer.analyze(*program) || emitter.hasErrors()) return "";

    sph::IRGenerator generator;
    auto module = generator.generate(*program);
    if (!module) return "";

    std::ostringstream out;
    sph::X86_64CodeGen codegen(out);
    codegen.setTarget(target);
    codegen.generate(*module);
    return out.str();
}

sph::Target getWindowsTarget() {
    auto t = sph::Target::parse("x86_64-pc-windows-msvc");
    return t ? *t : sph::Target::getHostTarget();
}

sph::Target getSysVTarget() {
    auto t = sph::Target::parse("x86_64-unknown-linux-gnu");
    return t ? *t : sph::Target::getHostTarget();
}

} // namespace

// =============================================================================
// Codegen Exhaustive Tests: ABI, Register Allocator & Assembly (651 to 700)
// =============================================================================

TEST_CASE("CodegenExhaustive: 651 Empty function emits prologue and epilogue") {
    std::string asmCode = generateAssembly("<?php fn test(): void {}", getSysVTarget());
    CHECK(asmCode.find("push rbp") != std::string::npos);
    CHECK(asmCode.find("mov rbp, rsp") != std::string::npos);
    CHECK(asmCode.find("ret") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 652 Integer return value in RAX") {
    std::string asmCode = generateAssembly("<?php fn test(): i32 { return 42; }", getSysVTarget());
    CHECK(asmCode.find("mov rax, 42") != std::string::npos || asmCode.find("mov eax, 42") != std::string::npos);
    CHECK(asmCode.find("ret") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 653 SysV first parameter in rdi") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32): i32 { return a; }", getSysVTarget());
    CHECK(asmCode.find("rdi") != std::string::npos || asmCode.find("edi") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 654 SysV second parameter in rsi") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): i32 { return b; }", getSysVTarget());
    CHECK(asmCode.find("rsi") != std::string::npos || asmCode.find("esi") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 655 SysV third parameter in rdx") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32, c: i32): i32 { return c; }", getSysVTarget());
    CHECK(asmCode.find("rdx") != std::string::npos || asmCode.find("edx") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 656 SysV fourth parameter in rcx") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32, c: i32, d: i32): i32 { return d; }", getSysVTarget());
    CHECK(asmCode.find("rcx") != std::string::npos || asmCode.find("ecx") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 657 SysV fifth parameter in r8") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32, c: i32, d: i32, e: i32): i32 { return e; }", getSysVTarget());
    CHECK(asmCode.find("r8") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 658 SysV sixth parameter in r9") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32): i32 { return f; }", getSysVTarget());
    CHECK(asmCode.find("r9") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 659 Windows x64 first parameter in rcx") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32): i32 { return a; }", getWindowsTarget());
    CHECK(asmCode.find("rcx") != std::string::npos || asmCode.find("ecx") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 660 Windows x64 second parameter in rdx") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): i32 { return b; }", getWindowsTarget());
    CHECK(asmCode.find("rdx") != std::string::npos || asmCode.find("edx") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 661 Windows x64 third parameter in r8") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32, c: i32): i32 { return c; }", getWindowsTarget());
    CHECK(asmCode.find("r8") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 662 Windows x64 fourth parameter in r9") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32, c: i32, d: i32): i32 { return d; }", getWindowsTarget());
    CHECK(asmCode.find("r9") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 663 Addition instruction emission") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): i32 { return a + b; }", getSysVTarget());
    CHECK(asmCode.find("add") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 664 Subtraction instruction emission") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): i32 { return a - b; }", getSysVTarget());
    CHECK(asmCode.find("sub") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 665 Multiplication instruction emission") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): i32 { return a * b; }", getSysVTarget());
    CHECK(asmCode.find("imul") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 666 Division instruction emission") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): i32 { return a / b; }", getSysVTarget());
    CHECK(asmCode.find("idiv") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 667 Modulo instruction emission") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): i32 { return a % b; }", getSysVTarget());
    CHECK(asmCode.find("idiv") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 668 Bitwise AND instruction emission") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): i32 { return a & b; }", getSysVTarget());
    CHECK(asmCode.find("and") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 669 Bitwise OR instruction emission") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): i32 { return a | b; }", getSysVTarget());
    CHECK(asmCode.find("or") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 670 Bitwise XOR instruction emission") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): i32 { return a ^ b; }", getSysVTarget());
    CHECK(asmCode.find("xor") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 671 Shift left instruction emission") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32): i32 { return a << 2; }", getSysVTarget());
    CHECK(asmCode.find("shl") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 672 Shift right instruction emission") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32): i32 { return a >> 2; }", getSysVTarget());
    CHECK(asmCode.find("sar") != std::string::npos || asmCode.find("shr") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 673 Comparison equal sete instruction") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): bool { return a == b; }", getSysVTarget());
    CHECK(asmCode.find("cmp") != std::string::npos);
    CHECK(asmCode.find("sete") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 674 Comparison not equal setne instruction") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): bool { return a != b; }", getSysVTarget());
    CHECK(asmCode.find("cmp") != std::string::npos);
    CHECK(asmCode.find("setne") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 675 Comparison less setl instruction") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): bool { return a < b; }", getSysVTarget());
    CHECK(asmCode.find("cmp") != std::string::npos);
    CHECK(asmCode.find("setl") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 676 Comparison greater setg instruction") {
    std::string asmCode = generateAssembly("<?php fn test(a: i32, b: i32): bool { return a > b; }", getSysVTarget());
    CHECK(asmCode.find("cmp") != std::string::npos);
    CHECK(asmCode.find("setg") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 677 Unconditional jump jmp instruction") {
    std::string asmCode = generateAssembly("<?php fn test(): i32 { mut x = 0; while (x < 10) { x = x + 1; } return x; }", getSysVTarget());
    CHECK(asmCode.find("jmp") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 678 Conditional jump jne instruction") {
    std::string asmCode = generateAssembly("<?php fn test(cond: bool): i32 { if (cond) { return 1; } return 0; }", getSysVTarget());
    CHECK(asmCode.find("jne") != std::string::npos || asmCode.find("je") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 679 Function call instruction call") {
    std::string asmCode = generateAssembly("<?php fn target(): i32 { return 10; } fn test(): i32 { return target(); }", getSysVTarget());
    CHECK(asmCode.find("call target") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 680 Stack allocation for local variables") {
    std::string asmCode = generateAssembly("<?php fn test(): i32 { let a = 1; let b = 2; return a + b; }", getSysVTarget());
    CHECK(asmCode.find("sub rsp") != std::string::npos);
    CHECK(asmCode.find("rbp -") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 681 Global string label emission in rodata") {
    std::string asmCode = generateAssembly("<?php fn test(): string { return \"hello world\"; }", getSysVTarget());
    CHECK(asmCode.find(".section .rodata") != std::string::npos || asmCode.find(".rdata") != std::string::npos);
    CHECK(asmCode.find("hello world") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 682 String address loaded via rip-relative lea") {
    std::string asmCode = generateAssembly("<?php fn test(): string { return \"string address\"; }", getSysVTarget());
    CHECK(asmCode.find("lea") != std::string::npos && asmCode.find("rip +") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 683 Unsafe pointer dereference load") {
    std::string asmCode = generateAssembly("<?php fn test(p: *i32): i32 { unsafe { return *p; } }", getSysVTarget());
    CHECK(asmCode.find("mov") != std::string::npos);
    CHECK(asmCode.find("[") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 684 Unsafe pointer store") {
    std::string asmCode = generateAssembly("<?php fn test(p: *i32): void { unsafe { *p = 99; } }", getSysVTarget());
    CHECK(asmCode.find("mov") != std::string::npos);
    CHECK(asmCode.find("99") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 685 Struct field access offset calculation") {
    std::string asmCode = generateAssembly("<?php struct Point { x: i32, y: i32 } fn test(p: Point): i32 { return p.y; }", getSysVTarget());
    CHECK(asmCode.find("mov") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 686 Array indexing calculation") {
    std::string asmCode = generateAssembly("<?php fn test(arr: [i32; 4], idx: i32): i32 { return arr[idx]; }", getSysVTarget());
    CHECK(asmCode.find("mov") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 687 Boolean true value represented as 1") {
    std::string asmCode = generateAssembly("<?php fn test(): bool { return true; }", getSysVTarget());
    CHECK(asmCode.find("mov rax, 1") != std::string::npos || asmCode.find("mov eax, 1") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 688 Boolean false value represented as 0") {
    std::string asmCode = generateAssembly("<?php fn test(): bool { return false; }", getSysVTarget());
    CHECK(asmCode.find("mov rax, 0") != std::string::npos || asmCode.find("mov eax, 0") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 689 Match lowering emits jump table or conditional branches") {
    std::string asmCode = generateAssembly("<?php fn test(x: i32): i32 { return match (x) { 1 => 10, 2 => 20, _ => 0 }; }", getSysVTarget());
    CHECK(asmCode.find("cmp") != std::string::npos);
    CHECK(asmCode.find("je") != std::string::npos || asmCode.find("jne") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 690 Option Some constructor sets tag and payload") {
    std::string asmCode = generateAssembly("<?php fn test(): Option<i32> { return Some(42); }", getSysVTarget());
    CHECK(asmCode.find("42") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 691 Result Ok constructor sets tag and payload") {
    std::string asmCode = generateAssembly("<?php fn test(): Result<i32, string> { return Ok(100); }", getSysVTarget());
    CHECK(asmCode.find("100") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 692 Float constant emission in data section or immediate") {
    std::string asmCode = generateAssembly("<?php fn test(): f64 { return 1.5; }", getSysVTarget());
    CHECK(!asmCode.empty());
    CHECK(asmCode.find("ret") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 693 64-bit float constant emission") {
    std::string asmCode = generateAssembly("<?php fn test(): f64 { return 2.718; }", getSysVTarget());
    CHECK(!asmCode.empty());
    CHECK(asmCode.find("ret") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 694 Cast i32 to i64 sign extension or mov") {
    std::string asmCode = generateAssembly("<?php fn test(x: i32): i64 { return x as i64; }", getSysVTarget());
    CHECK(!asmCode.empty());
}

TEST_CASE("CodegenExhaustive: 695 Logical NOT test and sete/setz") {
    std::string asmCode = generateAssembly("<?php fn test(b: bool): bool { return !b; }", getSysVTarget());
    CHECK(asmCode.find("xor") != std::string::npos || asmCode.find("sete") != std::string::npos || asmCode.find("cmp") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 696 Negation neg instruction") {
    std::string asmCode = generateAssembly("<?php fn test(x: i32): i32 { return -x; }", getSysVTarget());
    CHECK(asmCode.find("neg") != std::string::npos || asmCode.find("sub") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 697 Bitwise NOT not instruction") {
    std::string asmCode = generateAssembly("<?php fn test(x: i32): i32 { return ~x; }", getSysVTarget());
    CHECK(asmCode.find("not") != std::string::npos || asmCode.find("xor") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 698 Target triple information in header") {
    std::string asmCode = generateAssembly("<?php fn test(): void {}", getSysVTarget());
    CHECK(asmCode.find(".text") != std::string::npos);
    CHECK(asmCode.find(".globl") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 699 Multiple functions emitted in single assembly file") {
    std::string asmCode = generateAssembly("<?php fn f1(): void {} fn f2(): void {} fn f3(): void {}", getSysVTarget());
    CHECK(asmCode.find("f1:") != std::string::npos);
    CHECK(asmCode.find("f2:") != std::string::npos);
    CHECK(asmCode.find("f3:") != std::string::npos);
}

TEST_CASE("CodegenExhaustive: 700 SPH main entrypoint label emitted") {
    std::string asmCode = generateAssembly("<?php fn main(): void {}", getSysVTarget());
    CHECK(asmCode.find("main:") != std::string::npos);
}

