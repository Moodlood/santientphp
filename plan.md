# SantientPHP --- Language and Compiler Development Plan

**Project name:** SantientPHP\
**Short name:** SPH\
**Source extension:** `.sph`\
**Compiler executable:** `sphc`\
**Project manifest:** `sph.toml`\
**Specification baseline:** SPH 0.1\
**Status:** Proposed design and implementation roadmap

------------------------------------------------------------------------

## 1. Vision

SantientPHP is an independent, PHP-inspired programming language
intended to combine the approachable syntax and productivity of PHP with
the capabilities of a native systems language.

The project is not simply a PHP interpreter extension, transpiler, or
modified PHP runtime. It is intended to have its own:

-   Language specification and grammar.
-   Lexer, parser, semantic analyzer and type checker.
-   Intermediate representation (IR).
-   Compiler and native code-generation backend.
-   Runtime and standard library.
-   Command-line toolchain.
-   Tests, documentation and eventually package-management and editor
    tooling.

The language should support three programming styles:

1.  **Dynamic mode:** PHP-inspired syntax, dynamic values and managed
    runtime behavior.
2.  **Static mode:** statically typed, compiled code with predictable
    semantics.
3.  **Systems mode:** low-level memory access, pointers, explicit
    layouts, FFI and operating-system integration.

The three styles must coexist without forcing dynamic PHP semantics into
native code.

### Primary design principles

-   Familiar syntax, independent semantics.
-   Native compilation without requiring a PHP installation.
-   Safe-by-default native programming.
-   Explicit and auditable unsafe operations.
-   Multiple memory-management strategies with clear boundaries.
-   A custom typed IR between the frontend and LLVM.
-   Incremental implementation backed by tests.
-   Honest performance and compatibility claims based on measurements.

------------------------------------------------------------------------

## 2. Identity and initial technical decisions

  Item                      Initial decision
  ------------------------- --------------------------------------
  Language                  SantientPHP
  Abbreviation              SPH
  Source files              `.sph`
  Compiler command          `sphc`
  Project manifest          `sph.toml`
  Specification             SPH 0.1
  Compiler implementation   C++20
  Build system              CMake
  Native backend            LLVM
  Initial platform          Windows x86-64
  Later platforms           Linux and macOS
  Initial output            Native Windows executable
  Test framework            GoogleTest or Catch2
  Initial parser            Hand-written recursive descent
  Initial IR                Custom, typed and target-independent

C++23 may be adopted later if it provides a concrete benefit and is
supported by the chosen toolchain. Do not make the compiler depend on
unnecessary language features.

### Naming consistency

Use these names consistently in code, documentation and commands:

-   `SantientPHP` for the language.
-   `SPH` for short references and version labels.
-   `.sph` for source files.
-   `sphc` for the compiler executable.
-   `sph.toml` for project configuration.

Do not use the previous working name PHP-X in new files, examples,
namespaces, project paths or user-facing documentation unless discussing
historical design decisions.

------------------------------------------------------------------------

## 3. Language modes

### 3.1 Dynamic mode

Dynamic mode provides a PHP-inspired programming experience.

Illustrative example:

``` php
<?php

$user = "Kelvin";
$age = 25;

echo "Hello, " . $user;
```

The language specification must define:

-   Dynamic value representation.
-   Variable assignment and reassignment.
-   Runtime type conversions.
-   Array and map behavior.
-   Object and reference behavior.
-   Function calls and dynamic dispatch.
-   Exceptions and runtime errors.
-   String interpolation and concatenation.
-   Null and boolean conversion rules.

PHP compatibility must be measured against actual PHP behavior. Similar
syntax alone is not compatibility.

### 3.2 Static mode

Static mode provides predictable, statically checked native code.

``` php
<?php

fn add(a: i32, b: i32): i32 {
    return a + b;
}

fn main(): i32 {
    let first: i32 = 100;
    let second: i32 = 200;
    let result = add(first, second);

    println(result);
    return 0;
}
```

Proposed declarations:

-   `let`: immutable binding.
-   `mut`: mutable binding.
-   Type inference where the type is unambiguous.
-   Explicit function parameter and return types.
-   Checked conversions for potentially unsafe narrowing or
    dynamic-to-static conversion.

### 3.3 Systems mode

Systems mode adds features that need explicit control over
representation and memory:

-   Raw pointers and pointer arithmetic.
-   Explicit allocation and deallocation.
-   Struct layout and alignment.
-   C ABI compatibility.
-   Unions and bit-level operations.
-   Atomics and native threading.
-   SIMD and controlled inline assembly.
-   OS and hardware interfaces.

Unsafe operations must be visibly marked and restricted to explicit
`unsafe` regions.

------------------------------------------------------------------------

## 4. Type-system proposal

### 4.1 Primitive types

  Group                    Types
  ------------------------ ---------------------------
  Signed integers          `i8`, `i16`, `i32`, `i64`
  Unsigned integers        `u8`, `u16`, `u32`, `u64`
  Floating point           `f32`, `f64`
  Boolean                  `bool`
  Character                `char`
  Text                     `string`
  Pointer-sized integers   `usize`, `isize`
  Unit/no return           `void`
  Dynamic value            `mixed`
  Nullable type            `?T`

The specification must define integer overflow, signedness,
floating-point behavior, character encoding, string encoding,
nullability and conversion rules.

### 4.2 Composite types

Planned types:

-   Fixed-size arrays.
-   Slices.
-   Tuples.
-   Structs.
-   Enums.
-   Unions.
-   Maps and collections.
-   Generic types.

Illustrative struct:

``` php
struct Vector3 {
    x: f32,
    y: f32,
    z: f32,
}
```

Layout attributes may include:

``` php
#[repr(C)]
struct NativePoint {
    x: f32,
    y: f32,
}
```

Do not promise C-compatible layout until size, alignment, padding, field
offsets and ABI rules are implemented and tested.

### 4.3 Functions and modules

Planned features:

-   `fn` declarations.
-   Return types.
-   Type inference.
-   Closures and lambdas.
-   Default parameters.
-   Function overloading when statically resolvable.
-   Generics and monomorphization.
-   Modules and imports.
-   Visibility and namespaces.

### 4.4 Object-oriented programming

Planned features:

-   Classes and instances.
-   Constructors and destructors.
-   Interfaces and traits.
-   Inheritance.
-   Encapsulation and visibility.
-   Static members.
-   Method overriding.
-   Generics.
-   Carefully constrained operator overloading.

Dynamic-mode classes and native-layout types must not be treated as
interchangeable.

------------------------------------------------------------------------

## 5. Memory-management architecture

SantientPHP is intended to support multiple memory models. Their
semantics must be specified before implementation.

### 5.1 Automatic memory management

Planned runtime-managed capabilities:

-   Managed heap.
-   Reference counting.
-   Cycle detection and collection.
-   Automatic cleanup.
-   Managed strings and collections.

A cycle collector is not a first-release requirement. Define object
lifetime and runtime roots before implementing collection.

### 5.2 Manual memory management

Planned capabilities:

-   Allocation and deallocation APIs.
-   Raw pointers.
-   Pointer casts.
-   Pointer arithmetic in unsafe code.
-   Explicit initialization and destruction.
-   Allocation alignment and size rules.

### 5.3 Ownership and borrowing

Proposed rules:

-   One owner for owned memory.
-   Multiple simultaneous immutable borrows.
-   One exclusive mutable borrow.
-   Ownership transfer using an explicit move operation.
-   Lifetime and escape analysis.
-   No use-after-free or double-free in safe code.

### 5.4 Arena allocation

Provide arenas for workloads that allocate many objects with a common
lifetime. Support allocation, reset and release, with clear rules for
destructors and references that outlive the arena.

### 5.5 Memory-model boundaries

Managed objects must not expose unstable internal addresses as arbitrary
raw pointers. Interoperability should use handles, pinned memory or
documented ownership transfer.

Do not implement all memory models in SPH 0.1.

------------------------------------------------------------------------

## 6. Systems-programming capabilities

Planned capabilities and their prerequisites:

  Capability           Prerequisite
  -------------------- ----------------------------------------------
  Pointers             Type system, unsafe rules and memory model
  Struct layout        Type layout engine and ABI specification
  Bitwise operations   Integer semantics
  C FFI                ABI-compatible types and linker
  OS APIs              Platform abstraction and FFI
  Native threads       Runtime and synchronization model
  Atomics              Memory model and compiler ordering rules
  SIMD                 Vector types and target feature detection
  Inline assembly      Target-specific constraints and safety rules
  Networking           Standard library and OS integration

Inline assembly must define inputs, outputs, clobbers, memory effects,
architecture and dialect. Prefer compiler intrinsics and portable
abstractions when they meet the requirement.

------------------------------------------------------------------------

## 7. Error handling and concurrency

### 7.1 Error handling

Proposed features:

-   `Result<T, E>`.
-   Nullable types.
-   Error propagation using `?`.
-   Exceptions and `try/catch` where appropriate.
-   Debug symbols and stack traces.
-   Clear compile-time diagnostics.

Native code should favor explicit error propagation. Dynamic
compatibility mode should reproduce the documented exception behavior of
its supported PHP subset.

### 7.2 Concurrency

Implementation order:

1.  Native thread creation and joining.
2.  Mutexes and read/write locks.
3.  Atomic types and memory ordering.
4.  Channels and message passing.
5.  Thread pools.
6.  Futures and async/await.
7.  Advanced lock-free structures.

Safe shared mutable state must require synchronization or ownership
transfer. Define data-race behavior before exposing concurrency APIs.

------------------------------------------------------------------------

## 8. Compiler architecture

Do not translate source text directly into LLVM IR. Build a custom,
typed, verifiable SantientPHP IR.

### 8.1 Compilation pipeline

1.  Source manager.
2.  Lexer.
3.  Parser.
4.  Abstract syntax tree (AST).
5.  Semantic analyzer.
6.  Type checker.
7.  Ownership and safety analysis.
8.  SantientPHP IR generation.
9.  IR verification.
10. Optimization passes.
11. LLVM IR generation.
12. LLVM code generation.
13. Linking and executable packaging.

### 8.2 IR requirements

The IR should be:

-   Typed.
-   Target-independent where possible.
-   Verifiable.
-   Explicit about control flow.
-   Able to represent managed and native values.
-   Able to represent calls, memory operations and errors.
-   Suitable for optimization and debugging.

Dynamic-mode operations should have explicit runtime operations, for
example:

-   `DYNAMIC_ADD`
-   `DYNAMIC_GET_PROPERTY`
-   `DYNAMIC_CALL`
-   `DYNAMIC_ARRAY_ACCESS`

Do not lower dynamic operations as if they were ordinary native integer
or pointer operations.

### 8.3 Initial optimizations

Implement only after correctness:

-   Constant folding.
-   Dead-code elimination.
-   Basic block simplification.
-   Function inlining when safe.
-   Common subexpression elimination.
-   Escape analysis.
-   Loop optimizations.
-   Link-time optimization and profile-guided optimization later.

Expose optimization levels such as `-O0`, `-O1`, `-O2`, `-O3`, `-Os` and
`-Oz` only as the backend and compiler pipeline support them.

------------------------------------------------------------------------

## 9. Runtime and standard library

Suggested runtime modules:

``` text
runtime/
├── core/
├── memory/
├── managed_heap/
├── objects/
├── strings/
├── collections/
├── exceptions/
├── threads/
├── async/
└── compatibility/
```

Suggested standard library namespaces:

``` text
std::core
std::io
std::memory
std::math
std::os
std::network
std::thread
std::ffi
std::collections
```

The runtime must be modular. A small native program should not
automatically bundle the complete compatibility runtime, garbage
collector, networking stack and async scheduler.

------------------------------------------------------------------------

## 10. PHP compatibility subsystem

PHP compatibility should be a separate frontend/runtime with an
explicitly documented compatibility target.

Planned stages:

1.  PHP-inspired syntax and core constructs.
2.  Dynamic values, arrays, objects and conversions.
3.  References, exceptions and reflection subset.
4.  Common standard-library functions.
5.  PDO, MySQLi and cURL interoperability.
6.  Existing extension bridge where technically feasible.
7.  Composer ecosystem support.
8.  Compatibility testing against real PHP applications.

Do not claim full PHP 8.x compatibility until a documented compatibility
test suite passes. Record unsupported syntax, functions, extensions and
semantic differences.

------------------------------------------------------------------------

## 11. Command-line interface and project configuration

### 11.1 Compiler commands

``` powershell
sphc build main.sph
sphc run main.sph
sphc check main.sph
sphc test
sphc clean
sphc fmt
sphc doc
sphc inspect main.sph
```

Potential build options:

``` powershell
sphc build main.sph --release -O3
sphc build main.sph --target x86_64-windows
sphc build main.sph -o application.exe
```

### 11.2 Project manifest

Proposed `sph.toml`:

``` toml
[project]
name = "hello"
version = "0.1.0"
entry = "src/main.sph"

[compiler]
language_version = "0.1"

[target]
triple = "x86_64-windows"

[build]
mode = "debug"
optimization = "0"

[memory]
default = "automatic"

[compatibility]
php = false
```

This is a proposal, not a finalized schema. Validate configuration and
reject unknown or invalid values with useful diagnostics.

### 11.3 Target platforms

Initial:

-   `x86_64-windows`

Future:

-   `aarch64-windows`
-   `x86_64-linux`
-   `aarch64-linux`
-   `x86_64-macos`
-   `aarch64-macos`

------------------------------------------------------------------------

## 12. Implemented repository structure

``` text
santientphp/
├── compiler/              # Compiler frontend, AST, semantics, IR, and codegen
│   ├── ast/               # Structured AST node definitions and visitors
│   ├── codegen/           # Native x86_64 assembly, LLVM emitter, and Linker
│   ├── diagnostics/       # SourceSpan tracking and diagnostic reporting
│   ├── docgen/            # Docblock parser and HTML/Markdown generator
│   ├── doctor/            # Environment and toolchain health diagnostics
│   ├── formatter/         # AST-aware source code formatter
│   ├── ir/                # Custom typed intermediate representation (SPHIR)
│   ├── lexer/             # Token definitions and lexer
│   ├── lsp/               # Language Server Protocol 3.17 engine
│   ├── optimizer/         # Multi-pass IR optimization pipeline (7 passes)
│   ├── package/           # Project scaffolding and package manager
│   ├── parser/            # Recursive-descent parser
│   ├── semantic/          # Scopes, symbol tables, and semantic analyzer
│   ├── source/            # SourceManager and line-column mapping
│   ├── target/            # Target triples and platform abstractions
│   └── types/             # Static and dynamic type system
├── runtime/               # Modular C runtime libraries & Platform Abstraction Layer
│   ├── core/              # Memory allocators, arenas, Rc, strings, vectors, SIMD
│   ├── async/             # Atomics, threads, fibers, promises, and event loop
│   ├── compat/            # Dynamic sph_value, PHP arrays, PDO SQLite, cURL
│   └── pal.h              # Unified cross-platform OS abstraction (Win32 & POSIX)
├── stdlib/                # Normative native standard library (.sph modules)
│   ├── core.sph           # Math utilities, assertions, process exit
│   ├── io.sph             # Formatted console I/O
│   ├── fs.sph             # Filesystem operations and file handles
│   ├── collections.sph    # KeyValuePair, IntRange, range iterators
│   ├── async.sph          # Event loop yield, sleep_ms
│   └── net.sph            # HTTP networking primitives
├── editors/               # IDE integrations
│   └── vscode/            # Official VS Code extension and LSP client
├── docs/                  # Language specifications (0.1 & 1.0 Freeze), architecture, tutorials
├── examples/              # Positive language examples and feature demonstrations
├── tests/                 # 252 automated unit and integration tests (100% pass)
├── tools/sphc/            # Main compiler CLI driver
├── CMakeLists.txt         # Root build configuration
└── sph.toml               # Manifest configuration
```

------------------------------------------------------------------------

## 13. Development roadmap

### Phase 1 --- Specification and grammar (COMPLETED)

- [x] Define lexical rules and grammar (EBNF).
- [x] Define language modes (strict, transitional, dynamic).
- [x] Define declarations and expressions.
- [x] Define primitive types and conversion rules.
- [x] Define function and module syntax.
- [x] Define diagnostics and source locations.
- [x] Write positive and negative language examples.

**Deliverable:** SPH 0.1 and SPH 1.0 language specifications (`docs/specification/sph_0.1_spec.md`, `docs/specification/sph_1.0_spec.md`).

### Phase 2 --- Lexer and parser (COMPLETED)

- [x] Source manager with line and column indexing.
- [x] Token definitions and lexical scanner.
- [x] Recursive-descent parser for all statements and expressions.
- [x] Full AST node hierarchy and visitors.
- [x] Source-annotated diagnostics with caret underlines.
- [x] Lexer and parser unit tests.

**Deliverable:** `sphc check`, `sphc tokens`, and `sphc ast` parse `.sph` files and report diagnostics.

### Phase 3 --- Semantic analysis and type checking (COMPLETED)

- [x] Lexical scope management and symbol tables.
- [x] Function declarations, signatures, and call verification.
- [x] Primitive and compound type checking.
- [x] Type inference for `let` bindings.
- [x] Conversion and assignment validation.
- [x] Return-path analysis.

**Deliverable:** Invalid native programs are rejected before code generation.

### Phase 4 --- Custom IR (COMPLETED)

- [x] Custom typed intermediate representation (SPHIR) with 33 opcodes.
- [x] AST-to-IR lowering in `IRGenerator`.
- [x] In-memory `IRVerifier`.
- [x] SSA-like control flow with basic blocks and conditional branches.
- [x] 7-pass optimization pipeline in `PassManager`.

**Deliverable:** Correct, verifiable typed IR independent of backend tooling.

### Phase 5 --- Code Generation & Native Executable (COMPLETED)

- [x] Direct x86_64 assembly generator (`X86_64CodeGen`).
- [x] LLVM IR emitter (`LLVMEmitter`).
- [x] Native toolchain abstraction (`Linker`) driving MinGW-w64 / GCC / Clang / LD.
- [x] Fast compilation without mandatory LLVM developer libraries.
- [x] Native debug information (DWARF CFI).

**Deliverable:** `.sph` source compiles directly to a standalone Windows `.exe` binary.

### Phase 6 --- Native language expansion (COMPLETED)

- [x] Structs and enums with tagged variants.
- [x] Fixed arrays and dynamically sized slices.
- [x] Modules and multi-file imports (`import "...";`).
- [x] Classes, interfaces, methods, and single inheritance.
- [x] Generic structs and generic functions.

**Deliverable:** Statically typed, high-performance native language.

### Phase 7 --- Systems programming (COMPLETED)

- [x] Raw pointer types (`*const T`, `*mut T`).
- [x] Unsafe blocks (`unsafe { ... }`).
- [x] Memory layout and attributes (`#[repr(C)]`, `#[repr(packed)]`, `#[align(N)]`).
- [x] C FFI with external function bindings (`extern "C" fn`).

**Deliverable:** Native systems programs interact directly with C ABIs and system APIs.

### Phase 8 --- Safe and managed memory (COMPLETED)

- [x] Value types with deterministic stack allocation.
- [x] Reference-counted smart pointers (`Rc<T>`).
- [x] Unique heap allocation (`Box<T>`).
- [x] Region-based arena allocation (`Arena`).
- [x] Deterministic resource cleanup with LIFO `defer` statements.

**Deliverable:** Documented, tested memory-management models with leak-free execution.

### Phase 9 --- Runtime and standard library (COMPLETED)

- [x] Modular runtime split into `libsph_core.a`, `libsph_async.a`, and `libsph_compat.a`.
- [x] Cross-platform Platform Abstraction Layer (`runtime/pal.h`).
- [x] Normative native standard library in pure `.sph` (`stdlib/core.sph`, `stdlib/io.sph`, `stdlib/fs.sph`, `stdlib/collections.sph`, `stdlib/async.sph`, `stdlib/net.sph`).
- [x] Formatted I/O, filesystem access, collection iterators, and async yield primitives.

**Deliverable:** Full native applications run with zero third-party dependencies.

### Phase 10 --- PHP compatibility

- [x] Dynamic frontend and runtime (Milestone 10A: `dynamic` / `sph_value`, loose coercion, `sph.toml`).
- [x] PHP-style associative arrays (Milestone 10B: `sph_array`, mixed string/int keys, auto-vivification, `var_dump`).
- [x] PHP loops & core stdlib (Milestone 10C: `foreach`, `break`/`continue`, `count`, `array_*`, `str*`, `explode`/`implode`).
- [x] PHP dynamic & static objects (Milestone 10D: `new`, `->` operator, `$this`, `__construct`, `is_object`, `get_class`, `property_exists`).
- [x] Exceptions and references (Milestone 10E: `try`/`catch`/`finally`/`throw`, Exception hierarchy, references `&$var`).
- [x] Database and HTTP interoperability (Milestone 10F: `PDO` / SQLite via `winsqlite3.dll`, `curl_*` / `file_get_contents` via `winhttp.dll`).
- [x] Compatibility test suite (Milestone 10G: automated compat suite test_php_compat.cpp, examples/php_app.sph, spec boundaries Section 10.11).

**Deliverable:** a clearly bounded, tested subset of PHP programs works. (Phase 10 COMPLETED)

### Phase 11 --- Advanced native features

- [x] **Milestone 11A: Atomics, Memory Ordering & Native Threading** (COMPLETED)
  - First-class atomic types: `AtomicI32`, `AtomicI64`, `AtomicBool`, generic `Atomic<T>`.
  - Standard memory ordering models: `MemoryOrder::Relaxed`, `Acquire`, `Release`, `AcqRel`, `SeqCst`.
  - Hardware atomic operations: `load`, `store`, `fetch_add`, `fetch_sub`, `exchange`, `compare_exchange`.
  - Native OS threading via Windows `kernel32.dll`: `Thread::spawn`, `Thread::join`, `Thread::sleep_ms` (zero external 3rd-party binary dependencies).
  - Concurrency verified with `examples/atomics_threads.sph` and automated test suite.
- [x] **Milestone 11B: Async / Await & Cooperative Coroutine Runtime** (COMPLETED)
  - Stackful cooperative coroutines implemented using native Windows Fibers (`kernel32.dll`) with zero 3rd-party binary runtime dependencies.
  - First-class `Promise<T>` with `resolve`, `reject`, `is_ready`, `get_value`, and `await` lifecycle.
  - Cooperative event loop runtime: `EventLoop::spawn`, `EventLoop::yield_now`, and `EventLoop::run`.
  - Language syntax: `async fn` / `async function` declarations and `await <expr>` expressions.
  - Verification with `examples/async_coroutine.sph` and automated test suite.
- [x] **Milestone 11C: SIMD (Single Instruction, Multiple Data) & Target CPU Feature Detection** (COMPLETED)
  - First-class 128-bit vector value types: `f32x4`, `i32x4`, `f64x2`, `i64x2`.
  - Target CPU feature detection via CPUID: `Simd::has_sse`, `Simd::has_sse2`, `Simd::has_sse3`, `Simd::has_sse41`, `Simd::has_avx`, `Simd::has_avx2`, `Simd::has_neon`.
  - Constructors and factories: `new f32x4(x, y, z, w)`, `f32x4(x, y, z, w)`, `f32x4::splat(val)`, `f32x4::zero()`, `f32x4::load(ptr)`, `f32x4::store(ptr, vec)`.
  - Lane access: named components `v.x`, `v.y`, `v.z`, `v.w`, index expressions `v[i]`, and methods `v.get(i)` / `v.set(i, val)`.
  - Vector arithmetic operators (`+`, `-`, `*`, `/`) and method forms (`add`, `sub`, `mul`, `div`).
  - Parallel vector math: `v.sqrt()`, `v.abs()`, `v.min(other)`, `v.max(other)`.
  - Reductions and dot products: `v.sum()`, `v.dot(other)`.
  - Hardware SSE/SSE2 instruction set acceleration for x86_64 alongside portable scalar fallbacks with zero 3rd-party binary runtime dependencies.
  - Verified with `examples/simd_vectors.sph` and automated unit / end-to-end acceptance tests.
- [x] **Milestone 11D: Advanced Optimizations & Pass Manager Pipeline** (COMPLETED)
  - Optimization levels supported: `-O0`, `-O1`, `-O2`, `-O3`, `-Os` via CLI (`sphc -O<lvl>`) and manifest (`sph.toml` `optimization = ...`).
  - SPHIR Pass Manager pipeline (`PassManager`) executing module passes and iterative fixed-point function passes with IR verification.
  - Passes implemented:
    - `StoreToLoadForwardingPass`: Memory-to-register promotion forwarding stack alloca stores to loads (intra-block and entry-block single-definition promotion).
    - `ConstantFoldingPass`: Folds integer/float/bool arithmetic, logic, bitwise ops, comparisons, casts, and conditional branches.
    - `AlgebraicSimplificationPass`: Identity simplification (`x + 0 -> x`, `x * 1 -> x`), self-ops (`x - x -> 0`, `x / x -> 1`), annihilators (`x * 0 -> 0`), and strength reduction (`x * 2^k -> x << k`, `x / 2^k -> x >> k`).
    - `CommonSubexpressionEliminationPass`: Intra-block CSE with commutative matching (`a + b == b + a`).
    - `ControlFlowOptimizationPass`: Identical target branch collapsing and trampoline jump bypassing.
    - `DeadCodeEliminationPass`: Post-terminator dead code removal, unreachable basic block pruning, cascading dead instruction elimination.
    - `FunctionInliningPass`: Leaf function call-site inlining for small functions.
  - Linker integration with symbol stripping (`-s`) and linker-level optimization flags (`-Wl,-O1`, `-Wl,-O2`).
  - Verified with `examples/optimizations.sph`, optimizer unit tests (`tests/optimizer/test_optimizer.cpp`), and end-to-end acceptance tests (`tests/codegen/test_codegen.cpp`).
- [x] **Milestone 11E: Debugger Integration & DWARF / CFI Debug Information** (COMPLETED)
  - Native debug symbol generation with CLI flags `-g`, `-g0`, `-g1`, `-g2`, `-g3` and manifest configuration (`[build] debug = true`).
  - Source location propagation (`SourceLocation` with `fileId`, `line`, `col`) tracked from AST through SPHIR (`IRInstruction`, `IRFunction`, `IRModule` source file registry) down to native code emitters.
  - Assembly generation (`X86_64CodeGen`) emits DWARF line table directives (`.file`, `.loc`) mapping machine instructions to SPH source coordinates.
  - GNU Call Frame Information (CFI) emission (`.cfi_startproc`, `.cfi_def_cfa_offset`, `.cfi_offset`, `.cfi_def_cfa_register`, `.cfi_endproc`) enabling reliable call stack unwinding and backtraces.
  - LLVM IR debug metadata emission (`LLVMEmitter`) with `!llvm.dbg.cu`, `!DICompileUnit`, `!DIFile`, and `; !dbg` annotations.
  - Linker preservation of symbolic sections (`.debug_line`, `.debug_info`, `.debug_frame`, `.debug_abbrev`, `.eh_frame`) omitting symbol stripping when debug mode is enabled.
  - Verified with GNU GDB 17.1 (breakpoint resolution by symbol name, source line stepping, stack backtraces) on `examples/debug_example.sph`, automated unit tests (`tests/debug/test_debugger.cpp`), and end-to-end acceptance tests.
- [x] **Milestone 11F: Cross-Compilation Support (Target Triples & Toolchain Abstraction)** (COMPLETED)
  - First-class `Target` subsystem (`compiler/target/Target.hpp`, `Target.cpp`) supporting full parsing, validation, and canonical normalization of target triples.
  - Supported targets matrix:
    - `x86_64-windows` (`x86_64-w64-windows-gnu`): Microsoft Windows, MinGW/GNU ABI, Win64 calling convention (`rcx, rdx, r8, r9`, 32-byte shadow space).
    - `x86_64-linux` (`x86_64-unknown-linux-gnu`): Linux, System V AMD64 ABI (`rdi, rsi, rdx, rcx, r8, r9`), ELF `.rodata`, `.type @function`.
    - `aarch64-linux` (`aarch64-unknown-linux-gnu`): AArch64 (ARM64) Linux, AAPCS64 ABI, 64-bit alignment and data layouts.
    - `aarch64-macos` (`arm64-apple-macosx`): Apple Silicon macOS, Darwin ABI, Mach-O `.cstring` literals.
    - `x86_64-macos` (`x86_64-apple-darwin`): Intel macOS, System V AMD64 ABI, Darwin Mach-O sections.
    - `aarch64-windows` (`aarch64-pc-windows-msvc`): ARM64 Windows, MSVC ABI, PE/COFF sections.
  - Native assembly code generator (`X86_64CodeGen`) dynamically adapts:
    - Calling conventions: System V AMD64 ABI (`rdi, rsi, rdx, rcx, r8, r9`, no shadow space) vs Win64 ABI (`rcx, rdx, r8, r9`, 32-byte shadow space).
    - Data sections: `.section .rodata` (Linux ELF) vs `.section .rdata,"dr"` (Windows PE) vs `.section __TEXT,__cstring,cstring_literals` (macOS Mach-O).
    - Symbol directives: `.type <fn>, @function` (Linux) vs `.def <fn>; .scl 2; .type 32; .endef` (Windows).
  - LLVM IR generator (`LLVMEmitter`) emits target-specific `target datalayout = "..."` and `target triple = "..."` across all supported targets.
  - Linker toolchain abstraction (`Linker`): automatically discovers host compiler vs cross-compilers (`x86_64-linux-gnu-gcc`, `aarch64-linux-gnu-gcc`, `clang`) in PATH, links cross-binaries when cross-toolchain is available, and generates ready-to-link cross-target assembly (`.s`) and LLVM bitcode (`.ll`) with actionable target linking instructions.
  - Compiler CLI driver (`sphc`): added `-t <triple>`, `--target=<triple>`, `--target <triple>`, `targets` / `--list-targets`, manifest `[target] triple = "..."` validation.
  - Comprehensive test suite in `tests/target/test_target.cpp` (6 test cases, 111 assertions) and end-to-end acceptance tests.

**Deliverable:** advanced native programming capabilities. (Phase 11 COMPLETED)

### Phase 12 --- Developer ecosystem

- [x] **Milestone 12A: Code Formatter (`sphc fmt`)** (COMPLETED)
  - Dedicated formatter subsystem (`compiler/formatter/Formatter.hpp`, `Formatter.cpp`).
  - Indentation engine supporting configurable space/tab widths, nested block nesting, control flow structures, and preserving PHP opening tag (`<?php`).
  - Operator and punctuation spacing normalization: binary arithmetic, bitwise, logic, comparisons (`===`, `!==`, `==`, `!=`, `<=`, `>=`), assignment operators, and array/member arrows (`=>`, `->`, `::`).
  - Spacing rules for type annotations (`col: type`), commas, and control statements.
  - Safe comment preservation: single-line (`//`), multi-line (`/* ... */`), and docblocks (`/** ... */`).
  - Idempotent formatting guarantee (`format(format(x)) == format(x)`).
  - CLI operations in `sphc`:
    - `sphc fmt [file.sph]`: In-place code formatting.
    - `sphc fmt [file.sph] --check`: Non-destructive formatting verification for CI and pre-commit checks.
    - `sphc fmt [file.sph] --stdout`: Stream formatted code to standard output.
  - Verified with 5 comprehensive unit tests (23 assertions) in `tests/formatter/test_formatter.cpp` and end-to-end integration tests.

- [x] **Milestone 12B: Documentation Generator (`sphc doc`)** (COMPLETED)
  - Docblock and comment parser (`compiler/docgen/DocComment.hpp`, `DocComment.cpp`) supporting block docblocks (`/** ... */`) and triple-slash comments (`///`).
  - Tag extractor parsing `@brief`, `@param <name> <desc>`, `@return <desc>`, `@throws <type> <desc>`, `@see`, `@since`, `@author`, and paragraph descriptions.
  - AST doc extractor (`DocExtractor.hpp`, `DocExtractor.cpp`) mapping comments to functions, parameters, structs, classes, interfaces, and enums.
  - Multi-target generator (`DocGenerator.hpp`, `DocGenerator.cpp`):
    - GitHub-flavored Markdown generator (`--format=md`) with parameter tables, signatures, and module headers.
    - Standalone responsive HTML generator (`--format=html`) featuring dark-theme typography, sidebar symbol navigation, and real-time JavaScript search filtering.
  - CLI operations in `sphc`:
    - `sphc doc [file.sph|dir]`: Generates HTML/Markdown API documentation into `docs/` or custom `-o` output path.
    - Options: `--format=html|md`, `--title=<title>`, `-o <path>`.
  - Verified with 4 unit tests (40 assertions) in `tests/docgen/test_docgen.cpp` and end-to-end integration tests.

- [x] **Milestone 12C: Package & Project Manager (`sphc new`, `sphc init`, `sphc pkg`)** (COMPLETED)
  - Scaffolding engine (`compiler/package/PackageManager.hpp`, `PackageManager.cpp`) generating clean, production-ready project templates: `sph.toml`, `src/main.sph`, `.gitignore`, `README.md`.
  - In-place initialization (`sphc init`) for existing codebases without overwriting existing files.
  - Manifest dependency manager:
    - Path dependencies: `sphc pkg add <name> --path=<relpath>`
    - Git repository dependencies: `sphc pkg add <name> --git=<url>`
    - Dependency listing: `sphc pkg list`
  - Manifest parser and serializer integration (`compiler/manifest/Manifest.hpp`, `Manifest.cpp`) supporting `[dependencies]` table with round-trip parsing and validation.
  - Unit tests in `tests/package/test_package.cpp` (4 test cases, 18 assertions) and end-to-end integration tests.

- [x] **Milestone 12D: Language Server Protocol Engine (`sphc lsp`)** (COMPLETED)
  - Zero-dependency JSON and JSON-RPC 2.0 framing and parsing engine (`compiler/lsp/Json.hpp`).
  - Full LSP 3.17 server implementation (`compiler/lsp/LspServer.hpp`, `LspServer.cpp`) communicating over standard I/O streams:
    - Lifecycle: `initialize`, `initialized`, `shutdown`, `exit`.
    - Document sync: `textDocument/didOpen`, `textDocument/didChange` (full sync), `textDocument/didClose`.
    - Real-time compiler diagnostics: `textDocument/publishDiagnostics` publishing syntax and semantic diagnostics with precise 0-indexed line and column ranges and severity codes directly from `SourceManager`, `Parser`, and `SemanticAnalyzer`.
    - Hover documentation: `textDocument/hover` returning Markdown signatures and descriptions for keywords, types, SIMD vectors, atomic primitives, and AST-declared functions/structs/classes.
    - Autocompletion: `textDocument/completion` delivering context-aware keywords, native types, PHP standard library functions, and in-file declared symbols.
    - Go to Definition: `textDocument/definition` locating declarations of functions, structs, classes, and enums.
    - Symbol Outline: `textDocument/documentSymbol` generating symbol hierarchies for IDE navigation and breadcrumbs.
  - Verified with 6 unit tests (69 assertions) in `tests/lsp/test_lsp.cpp` and end-to-end stdio pipe tests.

- [x] **Milestone 12E: Health & Diagnostics (`sphc doctor`)** (COMPLETED)
  - Dedicated diagnostic subsystem (`compiler/doctor/Doctor.hpp`, `Doctor.cpp`).
  - Toolchain scanner detecting:
    - Host operating system and architecture (`Microsoft Windows x86_64`).
    - Host C/C++ compiler (`gcc` / `clang`) with full version identification.
    - Native linker (`ld` / Binutils).
    - Native debugger (`gdb`).
    - Version control toolchain (`git`).
    - Windows native subsystems: `kernel32.dll` (threading/fibers/atomics), `winhttp.dll` (cURL/HTTP), `winsqlite3.dll` (PDO/SQLite).
    - SantientPHP core runtime sources and precompiled static library (`runtime/core/runtime.c`, `libsph_runtime.a`).
    - Optional cross-compilation toolchains (`x86_64-linux-gnu-gcc`, `aarch64-linux-gnu-gcc`).
  - Clean, standard terminal output with operational status summaries.
  - Unit tests in `tests/doctor/test_doctor.cpp` (2 test cases, 12 assertions) and CLI integration tests.

**Deliverable:** SantientPHP 1.0 Developer Ecosystem. (Phase 12 COMPLETED)

------------------------------------------------------------------------

## 14. SPH 0.1: first vertical slice

The first release must prove the entire path from source file to native
executable.

### In scope

-   `.sph` source files.
-   PHP-style opening tag.
-   `let` and `mut`.
-   `i32`, `i64`, `f32`, `f64`, `bool` and `string` as feasible.
-   Type inference for simple declarations.
-   Functions and function calls.
-   Integer arithmetic.
-   `return`.
-   `if/else`.
-   `while`.
-   Basic `println`.
-   LLVM code generation.
-   Windows x86-64 executable linking.

### Out of scope

-   Full PHP compatibility.
-   Classes and inheritance.
-   Generic types.
-   Raw pointers.
-   Manual memory APIs.
-   Ownership and borrow checker.
-   Garbage collector.
-   Async/await.
-   SIMD and inline assembly.
-   Cross-platform support.

### Acceptance program

`examples/hello.sph`:

``` php
<?php

fn add(a: i32, b: i32): i32 {
    return a + b;
}

fn main(): i32 {
    let first: i32 = 100;
    let second: i32 = 200;
    let result = add(first, second);

    println(result);
    return 0;
}
```

Build:

``` powershell
sphc build examples/hello.sph --release -o hello.exe
```

Run:

``` powershell
.\hello.exe
```

Expected output:

``` text
300
```

Acceptance criteria:

1.  Compiler recognizes `.sph`.
2.  Lexer produces correct tokens.
3.  Parser builds the expected AST.
4.  Semantic analysis resolves functions and variables.
5.  Type checker rejects invalid operations.
6.  IR verifier accepts valid IR and rejects malformed IR.
7.  LLVM produces valid native code.
8.  Linker creates `hello.exe`.
9.  The executable runs without a PHP installation.
10. Invalid programs produce useful file, line and column diagnostics.

### Initial schedule estimate

  Week   Focus                       Demonstrable result
  ------ --------------------------- -----------------------------------
  1      Specification and grammar   SPH 0.1 design
  2      Lexer                       Token stream
  3      Parser and AST              Parsed syntax
  4      Semantic analysis           Scopes and symbols
  5      Type checker                Static validation
  6      IR                          Typed intermediate representation
  7      LLVM                        Native object generation
  8      Linker and CLI              First Windows executable

This is a planning estimate, not a guaranteed deadline. LLVM setup,
debugging and correctness work may extend it.

------------------------------------------------------------------------

## 15. Testing and quality strategy

Every compiler feature needs:

-   Unit tests.
-   Positive source examples.
-   Negative source examples.
-   Expected diagnostics.
-   Integration tests where relevant.

Test categories:

1.  Lexer token tests.
2.  Parser AST tests.
3.  Semantic and scope tests.
4.  Type-system tests.
5.  IR verifier tests.
6.  Code-generation tests.
7.  Executable integration tests.
8.  Runtime tests.
9.  Memory safety tests.
10. PHP compatibility tests, once that subsystem exists.

Use differential testing against PHP only for features explicitly
claimed to be PHP-compatible.

For native code, use sanitizers and debug builds where supported. Keep
release-mode benchmarks separate from correctness tests.

------------------------------------------------------------------------

## 16. Performance and compatibility claims

Do not claim that SantientPHP is as fast as C or C++ without
reproducible benchmarks.

Benchmark equivalent workloads, compiler versions, optimization levels
and hardware. Measure:

-   Execution time.
-   Memory use.
-   Binary size.
-   Compilation time.
-   Startup time.
-   Runtime overhead.

Likewise, do not claim complete PHP compatibility based on a few
successful examples. Publish a compatibility matrix and known
limitations.

------------------------------------------------------------------------

## 17. Immediate next actions

1.  Create the repository using the `santientphp/` name.
2.  Record this plan and the agent guide in the repository.
3.  Select and document the supported C++ compiler and LLVM version.
4.  Create a minimal CMake project that builds `sphc`.
5.  Write the SPH 0.1 lexical and grammar specification.
6.  Implement source management and diagnostics.
7.  Implement lexer tests before adding parser complexity.
8.  Build the first parser and AST.
9.  Continue toward the first native executable.

**Definition of early success:** a small, tested compiler that builds a
native Windows executable from a `.sph` file. All later features should
build on that verified foundation.
