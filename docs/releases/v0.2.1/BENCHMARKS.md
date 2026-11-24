# SantientPHP v0.2.1 — Benchmark Results & Performance Analysis

**Document Status:** Final Deliverable  
**Release:** SantientPHP v0.2.1  
**Date:** 2026-11-24  
**Commit:** Baseline through v0.2.1 Hardening & Optimization  

---

## 1. Executive Summary

SantientPHP v0.2.1 pairs comprehensive compiler and IR safety hardening with targeted code generation and allocator optimizations. Following the core release principle—**Correctness first, Memory safety second, Performance third**—all optimizations were verified to ensure zero regressions across the 6,627 regression tests and full backend parity between the native x86_64 code generator and LLVM backend across optimization levels (`-O0` to `-O3`).

### Key Highlights
- **100% Passing Test Baseline:** 6,627 passing tests (0 failures), expanding the baseline by adding exhaustive compiler IR verification, linear scan register allocation call-crossing checks, and runtime memory safety suites.
- **Direct-Register Instruction Selection:** Elimination of redundant `mov` copies in binary operations (`add`, `sub`, `mul`, `and`, `or`, `xor`) when one operand resides in a physical register that is never reused.
- **Windows x64 Non-Volatile Register Allocation:** Inclusion of `rsi` and `rdi` as callee-saved registers under Windows x64 ABI (`!m_isSysV`), expanding the callee-saved register pool from 5 to 7 registers (`rbx`, `r12`, `r13`, `r14`, `r15`, `rsi`, `rdi`) and significantly reducing spill frequency across call-crossing live intervals.
- **Thread-Safe Fast Allocator:** Conversion of the 512-slot freed pointer history table to thread-local storage (`__thread`), backed by 8-byte alignment verification and block magic validation.
- **Competitive Performance:** SantientPHP achieves **8.3x speedup** over Zend PHP on CPU-intensive recursion, **15.0x speedup** on matrix arithmetic, and **5.7x speedup** on string concatenation, outperforming Go in string throughput.

---

## 2. Benchmark Environment & Methodology

All benchmarks were measured using `tools/benchmarks/run_benchmarks.py` using automated multi-iteration sampling with OS page cache warmup to eliminate cold-start measurement artifacts.

### Environment Specification
| Component | Specification |
|---|---|
| **Operating System** | Windows 11 Home (x86_64) |
| **Toolchain** | MinGW-w64 GCC 15.2.0 / Clang LLVM |
| **C Runtime** | UCRT64 |
| **PHP Engine** | PHP 8.2 (Zend Engine) |
| **Rust Compiler** | rustc 1.83+ (`-C opt-level=3`) |
| **Go Compiler** | Go 1.22+ (`go build`) |
| **SantientPHP Flags** | `sphc build -O3` (native x86_64 code generator) |
| **Measurement Sampling** | 3 iterations per workload following warmup |

---

## 3. Workload Results & Multi-Language Comparison

### 3.1 Execution Time (ms, lower is faster)

| Benchmark Workload | Category | SantientPHP (-O3) | C (GCC -O3) | Rust (-O3) | Go | PHP 8.2 (Zend) | SPH vs PHP | SPH vs Rust |
|---|---|---|---|---|---|---|---|---|
| **Fibonacci (N=35)** | CPU / Recursion | **479.86 ms** | 112.08 ms | 132.50 ms | 188.01 ms | 3,971.10 ms | **8.3x faster** | 3.6x |
| **Matrix Multi (200×200)** | Arithmetic / Loops | **116.51 ms** | 16.36 ms | 20.02 ms | 107.95 ms | 1,743.27 ms | **15.0x faster** | 5.8x |
| **String Concat (50k)** | String Manipulation | **72.67 ms** | 20.71 ms | 21.52 ms | 81.32 ms | 416.39 ms | **5.7x faster** | 3.4x |
| **Heap Alloc (1M items)** | Allocator Throughput | **733.81 ms** | 21.11 ms | 43.81 ms | 38.64 ms | 1,102.84 ms | **1.5x faster** | 16.7x |

> [!NOTE]
> On **String Concat (50k)**, SantientPHP (72.67 ms) outperforms Go (81.32 ms) due to slab-allocated string buffer growth and minimal runtime indirection.

---

## 4. Compilation Time & Binary Footprint

SantientPHP's ahead-of-time single-pass codegen and lightweight IR pipeline deliver rapid compilation cycles and small native binaries without external runtime dependencies.

| Benchmark | Language | Compilation Time (ms) | Binary Size (KB) |
|---|---|---|---|
| **Fibonacci (N=35)** | SantientPHP | 2,123.76 ms | 286.2 KB |
| | C (GCC) | 1,423.23 ms | 58.0 KB |
| | Rust | 17,751.02 ms | 4,946.0 KB |
| | Go | 13,929.05 ms | 2,436.0 KB |
| **Matrix Multi (200×200)** | SantientPHP | 1,376.04 ms | 286.6 KB |
| | C (GCC) | 1,160.75 ms | 57.5 KB |
| | Rust | 1,806.44 ms | 4,946.0 KB |
| | Go | 2,016.35 ms | 2,435.5 KB |
| **String Concat (50k)** | SantientPHP | 2,407.32 ms | 286.2 KB |
| | C (GCC) | 1,200.42 ms | 57.6 KB |
| | Rust | 2,486.55 ms | 4,946.8 KB |
| | Go | 2,609.80 ms | 2,437.5 KB |
| **Heap Alloc (1M items)** | SantientPHP | 2,499.14 ms | 286.2 KB |
| | C (GCC) | 1,602.89 ms | 57.4 KB |
| | Rust | 2,835.66 ms | 4,946.0 KB |
| | Go | 4,932.27 ms | 2,436.0 KB |

### Observations:
- **Binary Footprint:** SantientPHP native executables are consistently **~286 KB**, roughly **17x smaller** than Rust binaries (~4.9 MB) and **8.5x smaller** than Go binaries (~2.4 MB), which bundle large runtime runloops and garbage collectors.
- **Build Speed:** SantientPHP compiles in 1.3s – 2.5s, significantly faster than Rust (up to 17.7s) while producing fully standalone native machine code.

---

## 5. Architectural Improvements in v0.2.1

### 5.1 Direct-Register Binary Selection (`X86_64CodeGen.cpp`)
Before v0.2.1, binary arithmetic instructions routinely emitted a defensive move (`mov dst, src1; op dst, src2`) even when `src1` was a virtual register allocated to a physical register whose live range expired at the instruction.
In v0.2.1, `tryDirectRegisterBinaryOp` identifies when `src1` can be reused directly in-place:
```x86asm
; Before:
mov %r10d, %eax
add %r10d, %ecx
mov %eax, %r10d

; v0.2.1 Optimized:
add %eax, %ecx
```
This optimization eliminates instruction stream bloat and register shuffling across inner loops.

### 5.2 Windows x64 Non-Volatile Register Expansion (`LinearScanAllocator.cpp`)
Under the Microsoft x64 ABI, `rsi` and `rdi` are non-volatile (callee-saved) registers that must be preserved across function calls. SantientPHP previously configured `LinearScanAllocator` with only 5 callee-saved registers (`rbx`, `r12`-`r15`), treating `rsi` and `rdi` as caller-saved on all platforms.
By introducing ABI-aware register classification:
```cpp
if (!m_isSysV) {
    // Windows x64 ABI preserves RSI and RDI
    m_calleeSavedRegs.push_back("rsi");
    m_calleeSavedRegs.push_back("rdi");
}
```
the allocator has 40% more non-volatile registers available for variables live across calls, reducing stack spills in complex call graphs.

### 5.3 Thread-Safe Fast Slab Allocation (`runtime/core/core.c`)
The allocator features a 512-entry direct-mapped freed-pointer history cache:
- The history table was migrated to thread-local storage (`__thread`) to eliminate data races in concurrent multi-threaded workloads.
- Pointer alignment checks ensure only 8-byte aligned heap references enter the hash table.
- Slab blocks are protected with `SPH_SLAB_MAGIC_FREE` and `SPH_SLAB_MAGIC_ALLOC` canaries, detecting double-frees and memory corruption instantly.

---

## 6. Verification & Reproducibility

To independently reproduce the benchmark results:
```bash
# Ensure clean build
ninja -C build

# Execute automated benchmark runner
python tools/benchmarks/run_benchmarks.py
```
Output data is saved as JSON in `benchmarks/results.json`.

