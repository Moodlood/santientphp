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
  - SIMD Operations: `SimdConstruct`, `SimdExtractLane`, `SimdInsertLane`, `SimdAdd`, `SimdSub`, `SimdMul`, `SimdDiv`, `SimdDot`.
  - Concurrency: `AtomicLoad`, `AtomicStore`, `AtomicFetchAdd`, `AtomicCompareExchange`.
  - PHP Dynamic Bridge: `CallDynamic`, `DynamicCoerce`, `ArrayGet`, `ArraySet`.
- **`IRVerifier`**: Enforces CFG and instruction invariants prior to optimization and code generation.

---

## 4. Optimization Pipeline (`compiler/optimizer/`)

The `PassManager` executes fixed-point iterative passes:
1. `StoreToLoadForwardingPass`: Promotes stack allocations into virtual registers.
2. `ConstantFoldingPass`: Evaluates constant arithmetic, logic, and branch targets at compile time.
3. `AlgebraicSimplificationPass`: Applies identities ($x + 0 \to x$, $x \times 1 \to x$, $x \times 0 \to 0$) and strength reduction ($x \times 2^k \to x \ll k$).
4. `CommonSubexpressionEliminationPass`: Eliminates redundant duplicate computations within basic blocks.
5. `ControlFlowOptimizationPass`: Prunes unreachable blocks and collapses redundant jump trampolines.
6. `DeadCodeEliminationPass`: Cascades dead instruction elimination.
7. `FunctionInliningPass`: Inlines small leaf functions directly at call sites.

---

## 5. Code Generation & Toolchain Abstraction (`compiler/codegen/`)

### Direct Native Generator (`X86_64CodeGen`)
- Generates clean, comment-annotated GNU assembly (`.s`).
- Implements Win64 calling convention (`rcx, rdx, r8, r9`, 32-byte shadow space) on Windows and System V AMD64 ABI on Linux.
- Emits DWARF line tables (`.file`, `.loc`) and GNU CFI directives (`.cfi_startproc`, `.cfi_def_cfa_offset`, `.cfi_endproc`) for full GDB call stack backtracing.

### LLVM IR Generator (`LLVMEmitter`)
- Produces valid, standardized LLVM assembly (`.ll`) annotated with target datalayouts and canonical triples.

### Linker (`Linker`)
- Discovers host tools (`gcc`, `clang`, `ld`) and target cross-compilers (`x86_64-linux-gnu-gcc`, `aarch64-linux-gnu-gcc`).
- Links against the modular runtime library `libsph_runtime.a` with automatic retry logic for Windows file locking.

