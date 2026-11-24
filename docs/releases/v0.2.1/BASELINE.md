# SantientPHP v0.2.1 — Baseline Verification Report

**Date:** November 2026  
**Status:** Reproducible Baseline Established  
**Commit/Target:** SantientPHP v0.2.1 Stabilization Phase  

---

## 1. System & Environment Specifications

- **Operating System:** Microsoft Windows 10 Pro (Build 19045 / x86_64)
- **CPU:** Intel(R) Core(TM) i5-4210U CPU @ 1.70GHz (2 Cores, 4 Logical Processors)
- **Host Architecture:** `x86_64-w64-mingw32`
- **Compiler Toolchain:** GCC 15.2.0 (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r5)
- **Build System:** CMake 4.4.3 with Ninja 1.13.2
- **C++ Standard:** C++20 (`set(CMAKE_CXX_STANDARD 20)`)
- **Optimization Flags:** `-O3` (benchmark release), default debug/testing configuration for test suite

---

## 2. Regression Test Suite Baseline

The entire regression test suite was executed via `build/bin/sph_tests.exe`:

```text
========================================
 Running 6625 test case(s)...
========================================
...
========================================
 Test Summary: 6625 passed, 0 failed.
========================================
```

- **Total Test Cases Executed:** 6,625
- **Passed:** 6,625 (100%)
- **Failed:** 0
- **Reported Figure Verification:** The reported figure of 6,625 passed tests with zero failures has been independently reproduced and verified against the actual test suite executable.

---

## 3. Benchmark Baseline (-O3)

All four benchmark workloads were compiled with `sphc build -O3` and executed directly under controlled conditions:

| Benchmark Workload | Source File | Purpose | Verification Output | Status |
|---|---|---|---|---|
| **Fibonacci (N=35)** | `benchmarks/fibonacci/fib.sph` | Deep recursion and integer arithmetic | `9227465` | **Passed** |
| **Matrix Multi (200×200)** | `benchmarks/matrix_mult/matmul.sph` | Nested loops, integer arrays, arithmetic | `-509789952` | **Passed** |
| **String Concat (50k)** | `benchmarks/string_concat/strings.sph` | String allocation and concatenation throughput | `50000` | **Passed** |
| **Heap Alloc (1M items)** | `benchmarks/memory_alloc/alloc.sph` | High-churn heap allocation and deallocation | `1783293664` | **Passed** |

### Benchmark Execution Times (Baseline Record)

Preserved from `benchmarks/results.json`:

```json
[
  {
    "name": "Fibonacci (N=35)",
    "sphc": { "compile_ms": 2732.18, "exec_ms": 326.21, "bin_size_kb": 282.2 }
  },
  {
    "name": "Matrix Multi (200x200)",
    "sphc": { "compile_ms": 2889.27, "exec_ms": 91.95, "bin_size_kb": 282.5 }
  },
  {
    "name": "String Concat (50k)",
    "sphc": { "compile_ms": 1807.69, "exec_ms": 47.73, "bin_size_kb": 282.2 }
  },
  {
    "name": "Heap Alloc (1M items)",
    "sphc": { "compile_ms": 1688.60, "exec_ms": 502.53, "bin_size_kb": 282.1 }
  }
]
```

---

## 4. Acceptance Criteria Verification

- [x] A clean build completes successfully.
- [x] The regression suite runs and matches 6,625 passed tests (0 failed).
- [x] All four benchmarks compile and produce valid, matching outputs.
- [x] Results are reproducible from documented commands.
- [x] Baseline record is committed and preserved in `docs/releases/v0.2.1/BASELINE.md`.

