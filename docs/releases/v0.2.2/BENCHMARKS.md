# SantientPHP v0.2.2 — Benchmark Results & Performance Analysis

**Document Status:** Automated Release Benchmark Report  
**Release:** SantientPHP v0.2.2  
**Date:** 2026-11-25  
**Source of Truth:** Generated directly from `benchmarks/results.json`

---

## 1. Execution, Compilation & Footprint Summary

### Execution Time (milliseconds, warmed median — lower is faster)

| Benchmark | Workload / Category | SantientPHP (-O3) | C (GCC -O3) | Rust (-O3) | Go (go build) | PHP 8.2 (Zend) | SPH vs PHP | SPH vs Rust |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| [**Fibonacci (N=35)**](benchmarks/fibonacci/) | CPU / Deep Recursion | **194.71 ms** | 66.38 ms | 83.84 ms | 144.05 ms | 5,887.10 ms | **30.2x faster** | 2.3x slower |
| [**Matrix Multi (200x200)**](benchmarks/matrix_mult/) | Arithmetic & Loops | **35.99 ms** | 19.69 ms | 24.61 ms | 36.46 ms | 1,473.41 ms | **40.9x faster** | 1.5x slower |
| [**String Concat (50k)**](benchmarks/string_concat/) | String Manipulation | **37.62 ms** | 19.94 ms | 24.20 ms | 25.57 ms | 242.05 ms | **6.4x faster** | 1.6x slower |
| [**Heap Alloc (1M items)**](benchmarks/memory_alloc/) | Allocator & Memory | **28.61 ms** | 20.28 ms | 27.34 ms | 26.70 ms | 844.28 ms | **29.5x faster** | 1.0x slower |

*Environment: Windows x86_64. Warmed median across 5 runs. Generated automatically from `benchmarks/results.json` via `python tools/benchmarks/run_benchmarks.py --update-docs`.*

### Executable Binary Footprint & Single-File Compile Time (`-O3`)

| Benchmark | SantientPHP Binary | Compile Time (`-O3`) | C (`gcc -O3 -s`) Binary | Rust Binary | Go Binary |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Fibonacci (N=35)** | **37.0 KB** | **633.7 ms** | 19.0 KB | 4946.0 KB | 2436.0 KB |
| **Matrix Multi (200x200)** | **37.5 KB** | **801.4 ms** | 18.5 KB | 4946.0 KB | 2435.5 KB |
| **String Concat (50k)** | **37.0 KB** | **820.1 ms** | 18.5 KB | 4946.8 KB | 2437.5 KB |
| **Heap Alloc (1M items)** | **37.5 KB** | **894.4 ms** | 18.5 KB | 4946.0 KB | 2436.0 KB |

---

## 2. Release Optimization Highlights (v0.2.2)

- **Loop-Scoped Escape Analysis (M1):** Iteration-local `Box::new(...)` allocations in loops reuse an entry-block stack slot safely verified by `EscapeAnalysisPass` + `IRVerifier`.
- **x86_64 Codegen & Loop Passes (M2):** Immediate arithmetic (`add/sub/cmp reg, imm32`), 3-operand/scaled `lea`, frame-pointer omission (`-O1+`), minimal callee-saves, LSRA copy coalescing, Loop Strength Reduction (LSR), 4x unrolling (`-O3`), distributive factoring, and `sph_string_append` MRU TLS fast-path.
- **Modular Runtime & Section Stripping (M4):** Standalone console programs link strictly against modular `libsph_core.a` with `-ffunction-sections -fdata-sections` and `-s -Wl,--gc-sections`, shrinking standalone binaries under **60 KB** (`< 120 KB`) and single-file `-O3` builds under **1.0 s**.
