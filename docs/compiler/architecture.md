# SantientPHP Compiler Architecture

This document describes the internal architecture of the SantientPHP compiler (`sphc`), its pipeline stages, Intermediate Representation (SPHIR), optimization pipeline, code generation backends, and runtime layer.

---

## 1. Pipeline Overview

```
Source Code (.sph)
       │
       ▼
 ┌───────────┐
 │   Lexer   │ ──► Tokens (with SourceSpan file, line, col)
 └───────────┘
       │
       ▼
 ┌───────────┐
 │  Parser   │ ──► Abstract Syntax Tree (ProgramAST)
 └───────────┘
       │
       ▼
 ┌───────────────────┐
 │ Semantic Analyzer │ ──► Type Checking, Scope Resolution, Function Binding
 └───────────────────┘
       │
       ▼
 ┌───────────────────┐
 │   IR Generator    │ ──► Custom Typed Intermediate Representation (SPHIR)
 └───────────────────┘
       │
       ▼
 ┌───────────────────┐
 │    IR Verifier    │ ──► Validates Invariants (SSA forms, terminator rules)
 └───────────────────┘
       │
       ▼
 ┌───────────────────┐
 │   Pass Manager    │ ──► SPHIR Optimizations (Folding, CSE, Inlining, DCE)
 └───────────────────┘
       │
       ├───► Direct Native Backend: X86_64CodeGen ──► Machine Assembly (.s) with DWARF & CFI
       │                                                         │
       └───► LLVM Backend: LLVMEmitter ─────────────► LLVM IR (.ll)
                                                                 │
                                                                 ▼
                                                    ┌─────────────────────────┐
                                                    │  Linker (GNU ld/clang)  │ ──► Native Binary (.exe)
                                                    └─────────────────────────┘
```

---

## 2. Frontend Subsystems

### 2.1 Source Management & Diagnostics (`compiler/source/`, `compiler/diagnostics/`)
- `SourceManager`: Ingests source files or in-memory string buffers, assigns unique 32-bit `fileId` values, and performs fast $O(\log N)$ binary search line and column coordinate resolution.
- `SourceSpan`: Tracks source ranges `(start, end)` encompassing start/end line and column numbers.
- `DiagnosticEmitter`: Formats clean diagnostics with caret underline banners (`^^^^^`), collects structured diagnostics for the LSP server, and tracks error/warning tallies without cascading crashes.

### 2.2 Lexer & Tokens (`compiler/lexer/`)
- Generates a stream of strongly-typed `Token` objects.
- Differentiates keywords (`fn`, `let`, `mut`, `class`, `interface`, `async`, `await`, etc.), native types (`i32`, `f64`, `f32x4`, `AtomicI32`), PHP variables (`$var`), double-arrow operators (`=>`), and doc comments (`/** ... */`).

### 2.3 Recursive-Descent Parser (`compiler/parser/`)
- Builds strongly-typed AST nodes (`ProgramAST`, `DeclAST`, `StmtAST`, `ExprAST`).
- Features robust panic-mode recovery with statement synchronization tokens (`fn`, `class`, `struct`, `let`, `;`, `}`).

### 2.4 Semantic Analysis & Type System (`compiler/semantic/`, `compiler/types/`)
- Scoped symbol tables (`Scope`, `Symbol`).
- Enforces strict type compatibility for arithmetic, comparisons, conversions, and function arguments in static mode.
- Infers variable types deterministically (`let x = 42;` -> `i32`).
- Sum types and error handling: `Option<T>` (`Some(T)`, `None`), `Result<T, E>` (`Ok(T)`, `Err(E)`), and the `?` error propagation operator (`TryPropagateExpr`).
- Pattern matching analysis: exhaustiveness and reachability checking across `match` expressions (literals, ranges, wildcards, guards).
- **`BorrowChecker`**: Enforces single-mutable-writer / multi-reader ownership guarantees, verifying safe resource borrowing at compile time without a runtime garbage collector.
- Bridges dynamic PHP operations through `dynamic` type semantics with loose coercions.

---

## 3. SPHIR: Custom Intermediate Representation (`compiler/ir/`)

SantientPHP uses an explicit, typed intermediate representation rather than translating AST directly to machine code or LLVM:
- **`IRModule`**: Container for global declarations, function definitions, string literals, and source file mappings.
- **`IRFunction`**: Encapsulates a control flow graph (CFG) consisting of one or more `IRBasicBlock` nodes.
- **`IRBasicBlock`**: Sequence of non-terminating instructions ending with exactly one terminator (`Ret`, `Br`, `CondBr`).
- **`IRInstruction`**: Typed operations:
  - Integer arithmetic: `Add`, `Sub`, `Mul`, `Div`, `Mod`, `Neg`.
  - Bitwise & logic: `And`, `Or`, `Xor`, `Shl`, `Shr`, `Not`.
  - Comparisons: `Eq`, `Ne`, `Lt`, `Le`, `Gt`, `Ge`.
  - Memory: `Alloca`, `Load`, `Store`, `GetElementPtr`.
  - Control Flow: `Br`, `CondBr`, `Call`, `Ret`.
  - Pattern Matching: Multi-way branch chains lowered into arm blocks, guard evaluations, and merge blocks (`match.arm`, `match.merge`).
  - SIMD Operations: `SimdConstruct`, `SimdExtractLane`, `SimdInsertLane`, `SimdAdd`, `SimdSub`, `SimdMul`, `SimdDiv`, `SimdDot`.
  - Concurrency: `AtomicLoad`, `AtomicStore`, `AtomicFetchAdd`, `AtomicCompareExchange`.
  - High-Performance Strings: In-place geometric append lowering (`sph_string_append`).
  - PHP Dynamic Bridge: `CallDynamic`, `DynamicCoerce`, `ArrayGet`, `ArraySet`.
- **`IRVerifier`**: Enforces CFG and instruction invariants prior to optimization and code generation.

---

## 4. Optimization Pipeline (`compiler/optimizer/`)

The `PassManager` executes fixed-point iterative passes over SPHIR:
1. `StoreToLoadForwardingPass`: Promotes stack allocations into virtual registers.
2. `ConstantFoldingPass`: Evaluates constant arithmetic, logic, and branch targets at compile time.
3. `AlgebraicSimplificationPass`: Applies algebraic identities ($x + 0 \to x$, $x \times 1 \to x$, $x \times 0 \to 0$, $x - x \to 0$).
4. `StrengthReductionPass`: Replaces expensive arithmetic with cheaper equivalents (e.g. integer multiplication by power-of-two converted to bitwise shifts $x \times 2^k \to x \ll k$).
5. `CommonSubexpressionEliminationPass`: Eliminates redundant duplicate computations within basic blocks.
6. `LoopInvariantCodeMotionPass`: Hoists loop-invariant computations out of loops to reduce per-iteration CPU cycles.
7. `ControlFlowOptimizationPass`: Prunes unreachable blocks and collapses redundant jump trampolines.
8. `DeadStoreEliminationPass`: Eliminates stores to stack slots that are overwritten before being read.
9. `DeadCodeEliminationPass`: Cascades dead instruction elimination across the function CFG.
10. `FunctionInliningPass`: Inlines small leaf functions directly at call sites to eliminate call-overhead.

---

## 5. Code Generation & Toolchain Abstraction (`compiler/codegen/`)

### Direct Native Generator (`X86_64CodeGen`)
- Generates clean, comment-annotated GNU assembly (`.s`).
- Implements Win64 calling convention (`rcx, rdx, r8, r9`, 32-byte shadow space) on Windows and System V AMD64 ABI on Linux.
- **Compare-Branch Fusion**: Fuses comparisons directly with conditional jumps (`cmp` + `jcc`), bypassing intermediate boolean register flags (`setcc`), zero-extensions, and stack spill cycles.
- **Assembly Peephole Optimizer**: Second-stage pass scanning emitted basic blocks to eliminate redundant store-followed-by-load cycles (`mov [rbp - X], reg` followed by `mov reg, [rbp - X]`) and dead 64-bit self-moves.
- **Address-Of Function Pointers**: Emits RIP-relative effective address loads (`lea reg, [rip + fn_name]`) for function pointers (`&fn_name`), allowing SantientPHP functions to be passed directly as C callbacks (e.g. Win32 `WNDPROC`, `LPTHREAD_START_ROUTINE`).
- **C Struct & Union Layout**: Computes C-compatible alignments and offsets; unions map every member to offset 0, and `sizeof(T)` lowers to immediate constants.
- Emits DWARF line tables (`.file`, `.loc`) and GNU CFI directives (`.cfi_startproc`, `.cfi_def_cfa_offset`, `.cfi_endproc`) for full GDB call stack backtracing.

### LLVM IR Generator (`LLVMEmitter`)
- Produces valid, standardized LLVM assembly (`.ll`) annotated with target datalayouts and canonical triples.

### Linker (`Linker`)
- Discovers host tools (`gcc`, `clang`, `ld`) and target cross-compilers (`x86_64-linux-gnu-gcc`, `aarch64-linux-gnu-gcc`).
- In non-debug builds, passes `-s` (symbol stripping) and `-pipe` (in-memory compiler piping) to produce compact binaries (~226 KB).
- Forwards user-specified external library link flags (`-l<lib>`, e.g. `-luser32 -lgdi32 -lws2_32`) and library directories (`-L<dir>`).
- Links against the modular runtime library `libsph_runtime.a` with automatic retry logic for Windows file locking.

---

## 6. Runtime Performance Layer (`runtime/core/`, `runtime/net/`, `runtime/async/`)

- **Fast Slab Allocator**: Thread-local slab and freelist memory recycling for objects $\le 64$ bytes, eliminating global heap lock contention for high-frequency allocations.
- **Amortized $O(1)$ String Appends**: Geometric buffer capacity doubling (`cap *= 2`) tracked via thread-local string caches, eliminating quadratic reallocation bottlenecks in string-heavy loops.
- **Binary Buffer (`ByteBuffer`)**: Null-byte safe dynamic byte buffer type (`sph_byte_buffer`) for binary data manipulation without string truncation at `0x00` bytes.
- **RFC 6455 WebSocket Engine**: Zero-dependency RFC 6455 WebSocket client, framing parser, ping/pong heartbeats, and SHA-1/Base64 handshake calculation (`runtime/net/websocket.c`).
- **Native OS Threading & Mutexes**: Win32 (`CreateThread`, `CRITICAL_SECTION`) and POSIX (`pthread_create`, `pthread_mutex_t`) hardware concurrency.
- **Stackful Fibers & Event Loop**: Native Windows Fiber / POSIX context switching integrated with cooperative coroutines (`async fn`, `await`).

---

## 7. Platform Support & Compatibility Matrix

SantientPHP is engineered with a modular Platform Abstraction Layer (`runtime/pal.h`) ensuring consistent behavior across host operating systems while exploiting native OS primitives where available:

| Subsystem / Feature | Windows (`x86_64-pc-windows-gnu`) | Linux (`x86_64-unknown-linux-gnu`) | macOS (`aarch64-apple-darwin`) | Implementation Details |
| :--- | :--- | :--- | :--- | :--- |
| **Core Compiler (`sphc`)** | **Tier 1 (Full)** | **Tier 1 (Full)** | Experimental | Direct `X86_64CodeGen` & `LLVMEmitter`; Win64 & SysV AMD64 ABIs |
| **Systems Mode & Bare-Metal FFI** | **Tier 1 (Full)** | **Tier 1 (Full)** | Experimental | `#[repr(C)]`, typed raw pointers (`*T`), compile-time `sizeof`, DWARF & CFI |
| **Threading & Mutexes** | **Tier 1 (Full)** | **Tier 1 (Full)** | Experimental | Win32 (`CreateThread`) / POSIX (`pthread_create`) via `runtime/pal.h` |
| **Hardware Atomics & SIMD** | **Tier 1 (Full)** | **Tier 1 (Full)** | Experimental | `AtomicI32`/`AtomicI64`, 128-bit SSE2/AVX vectors (`f32x4`, `i32x4`) |
| **Fibers & Cooperative Coroutines** | **Tier 1 (Full)** | **Tier 1 (Full)** | Planned | Windows Fibers / POSIX `ucontext` + Event Loop runtime |
| **Networking & WebSockets** | **Tier 1 (Full)** | **Tier 1 (Full)** | Experimental | Zero-dependency RFC 6455; `WSAPoll` on Win32, POSIX `poll` on Linux |
| **Dynamic PHP Subsystem** | **Tier 1 (Full)** | **Tier 1 (Full)** | Experimental | `sph_value`, dynamic associative arrays, PDO SQLite, WinHTTP/cURL |
| **Native GUI Controls (`--gui`)** | **Tier 1 (Full)** | Headless / Planned | Headless / Planned | Win32 controls (`user32`, `comctl32` v6, Per-Monitor DPI v2); Linux GUI (X11/Wayland) on roadmap |
| **SIMD Framebuffer Canvas** | **Tier 1 (Full)** | **Tier 1 (Full)** | Experimental | 32-bit RGBA pixel surface, alpha blending, OpenGL context integration |
| **Toolchain Health (`sphc doctor`)** | **Tier 1 (Full)** | **Tier 1 (Full)** | Experimental | Verifies host GCC/Clang, `ld`, `gdb`, `git`, and platform-specific system runtime libraries |

> [!NOTE]
> On Linux, console applications, server engines, async fibers, high-performance SIMD, and Systems FFI are fully supported for production deployment. Compiling with `--gui` on Linux runs in headless/framebuffer canvas mode until the native X11/Wayland backend milestone lands.

