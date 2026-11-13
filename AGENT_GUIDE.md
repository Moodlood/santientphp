# SantientPHP --- AI Agent Rules and Development Guide

**Project:** SantientPHP\
**Short name:** SPH\
**Source extension:** `.sph`\
**Compiler:** `sphc`\
**Manifest:** `sph.toml`\
**Specification baseline:** SPH 0.1 & SPH 1.0 ([docs/specification/sph_1.0_spec.md](docs/specification/sph_1.0_spec.md))

This document defines how AI coding agents and automated contributors
must work in the SantientPHP repository.

------------------------------------------------------------------------

## 1. Mission

Build SantientPHP as an independent, PHP-inspired compiled programming
language with dynamic, static and systems-programming capabilities.

The project must have its own language specification, compiler,
intermediate representation, runtime, standard library, tests and
toolchain.

Do not treat SantientPHP as:

-   A PHP interpreter extension.
-   A simple PHP-to-C++ text converter.
-   A syntax-only wrapper around LLVM.
-   A project that already supports features that have only been
    proposed.
-   A language with complete PHP compatibility unless that has been
    demonstrated by tests.

The immediate engineering priority is a small, correct compiler that
turns `.sph` source into a native Windows executable.

------------------------------------------------------------------------

## 2. Naming and terminology rules

Use these names consistently:

-   Language: `SantientPHP`.
-   Abbreviation: `SPH`.
-   Source extension: `.sph`.
-   Compiler executable: `sphc`.
-   Manifest: `sph.toml`.

Do not rename the project or extensions without explicit
user authorization.

Use `sph_` or `sph::` prefixes for compiler-specific symbols where
appropriate. Choose one namespace convention and apply it consistently.

------------------------------------------------------------------------

## 3. Read before changing

Before implementing a task:

1.  Inspect the repository structure.
2.  Read `README.md`, the language specification and relevant
    architecture documentation.
3.  Read the applicable CMake configuration and existing build
    instructions.
4.  Inspect the relevant source files and tests.
5.  Identify the current implementation stage.
6.  Determine which behavior is implemented, partially implemented,
    planned or unsupported.
7.  Check existing conventions before introducing a new abstraction.

Do not assume the repository matches a proposed architecture document.
The actual source code is authoritative for current implementation; the
approved language specification is authoritative for intended language
semantics.

If a required file or toolchain is unavailable, report that limitation
instead of inventing its contents or claiming verification.

------------------------------------------------------------------------

## 4. Engineering principles

### 4.1 Correctness before features

-   Prefer a small working vertical slice over broad incomplete
    scaffolding.
-   Do not implement several major subsystems in one change unless
    explicitly requested.
-   Add tests with each feature.
-   Keep the compiler buildable after each meaningful change.
-   Never silently accept invalid programs.
-   Never generate incorrect native code to make a test pass.

### 4.2 Explicit boundaries

Keep these responsibilities separate:

-   Lexer: characters to tokens.
-   Parser: tokens to AST.
-   Semantic analyzer: names, scopes and declarations.
-   Type checker: type rules and conversions.
-   Ownership checker: ownership and borrowing rules when implemented.
-   IR generator: AST to SantientPHP IR.
-   IR verifier: validates IR invariants.
-   Optimizer: transforms valid IR while preserving behavior.
-   LLVM backend: SantientPHP IR to LLVM IR.
-   Linker: objects and libraries to final executable.
-   Runtime: services required by generated programs.

Do not put parsing logic in the LLVM backend or runtime behavior in the
lexer.

### 4.3 Avoid premature abstraction

-   Do not create interfaces for hypothetical future use without a
    concrete requirement.
-   Do not create empty placeholder modules for every roadmap feature.
-   Do not add dependencies unless they solve a demonstrated need.
-   Prefer small, cohesive components.
-   Avoid global mutable state.
-   Avoid hidden ownership and lifetime assumptions.

### 4.4 Preserve compatibility

-   Do not break existing syntax or semantics without an explicit
    specification change.
-   Add regression tests for bug fixes.
-   Document intentional breaking changes.
-   Keep dynamic-mode semantics separate from native-mode semantics.

------------------------------------------------------------------------

## 5. Language-design rules

### 5.1 Modes

SantientPHP has three intended styles:

1.  Dynamic PHP-inspired mode.
2.  Statically typed native mode.
3.  Low-level systems mode.

Do not assume that a feature implemented in one mode automatically
exists in another.

### 5.2 Dynamic mode

Dynamic mode may use PHP-inspired syntax such as:

``` php
<?php
$value = 100;
$value = "text";
```

Its runtime behavior must be explicitly specified. Do not approximate
PHP conversions or array behavior without documenting the differences.

### 5.3 Native mode

Native mode uses explicit or inferred static types:

``` php
let count: i32 = 10;
mut total: i64 = 0;
```

Rules:

-   `let` creates an immutable binding.
-   `mut` creates a mutable binding.
-   Type inference must be deterministic.
-   Implicit conversions must be safe and documented.
-   Narrowing conversions must be explicit or checked.
-   Unknown types must not silently become arbitrary native values.

### 5.4 Systems mode

Pointers, manual allocation, FFI, explicit layouts and inline assembly
are advanced features.

-   Safe code is the default.
-   Unsafe operations must be explicitly marked.
-   Pointer arithmetic must not appear in safe code unless the
    specification explicitly permits a proven-safe operation.
-   C ABI claims require layout and calling-convention tests.
-   Never expose managed object internals as stable native pointers
    without a documented pinning or handle mechanism.

### 5.5 Specification-first changes

When changing grammar, types, conversions, memory behavior or ABI
semantics:

1.  Update the language specification.
2.  Add examples.
3.  Add positive and negative tests.
4.  Implement the change.
5.  Update user and developer documentation.

Do not let implementation behavior accidentally become the language
specification.

------------------------------------------------------------------------

## 6. Compiler architecture rules

### 6.1 Frontend

-   Use a source manager that tracks filenames and source spans.
-   Tokens should carry source locations.
-   Diagnostics should report file, line, column and a useful message.
-   The parser should produce structured AST nodes.
-   Avoid retaining pointers into temporary source buffers.
-   Prefer explicit AST node kinds and well-defined ownership.

### 6.2 Semantic analysis and type checking

-   Use scoped symbol tables.
-   Detect duplicate declarations and unresolved names.
-   Check function arguments and return types.
-   Reject invalid operations before code generation.
-   Keep type inference separate from code generation.
-   Do not use LLVM types as the language's only type system.

### 6.3 Intermediate representation

SantientPHP must have its own typed IR between the frontend and LLVM.

The IR must:

-   Represent values and types explicitly.
-   Represent control flow clearly.
-   Distinguish dynamic runtime operations from native operations.
-   Define ownership and memory effects as those features are
    introduced.
-   Be verified before optimization and code generation.
-   Have tests independent of LLVM.

Do not translate source text directly into LLVM IR.

### 6.4 LLVM backend

-   Treat LLVM as a backend, not as the language frontend.
-   Keep LLVM-specific types and APIs inside the backend layer.
-   Do not assume host and target architectures are identical.
-   Validate target triples and data layouts.
-   Handle LLVM errors and diagnostics.
-   Do not claim cross-platform output until the relevant target
    toolchain is tested.
-   Pin and document the supported LLVM version.

### 6.5 Optimization

Optimization must preserve language semantics.

-   Keep an unoptimized debug path.
-   Add optimizations incrementally.
-   Test optimized and unoptimized output against the same expected
    behavior.
-   Do not optimize across dynamic operations unless their semantics are
    modeled correctly.
-   Do not prioritize benchmark numbers over correctness.

------------------------------------------------------------------------

## 7. Memory and safety rules

The long-term language design supports automatic memory management,
manual memory management, ownership/borrowing and arenas. These are not
all part of SPH 0.1.

When implementing memory features:

-   Document who owns every allocation.
-   Define initialization and destruction behavior.
-   Define nullability and pointer validity.
-   Define alignment and allocation size.
-   Prevent double-free and use-after-free in safe code.
-   Define behavior for allocation failure.
-   Keep managed and unmanaged object representations distinct.
-   Add tests for normal paths and failure paths.
-   Use sanitizers where supported.
-   Never describe a memory operation as safe unless its safety
    conditions are enforced or clearly documented.

Do not implement a partial borrow checker that silently accepts cases it
cannot analyze. Reject unsupported cases until their semantics are
implemented.

------------------------------------------------------------------------

## 8. Runtime and standard-library rules

-   Keep runtime functionality modular.
-   Avoid linking the full runtime into minimal native programs
    unnecessarily.
-   Keep platform-specific code behind clear interfaces.
-   Define error behavior for I/O and OS operations.
-   Avoid exposing unstable internal runtime structures as public ABI.
-   Test runtime functions independently of compiler parsing.
-   Document thread safety for shared runtime objects.

Do not claim that a standard-library module exists merely because its
directory or header has been created.

------------------------------------------------------------------------

## 9. PHP compatibility rules

PHP compatibility is a separate subsystem and a long-term objective.

-   State the exact PHP version or behavior subset being targeted.
-   Test behavior against the corresponding PHP implementation where
    possible.
-   Document unsupported syntax, functions, extensions and behavior.
-   Treat PHP arrays, references, object semantics and conversions
    deliberately.
-   Do not assume PHP arrays are equivalent to native fixed-size arrays.
-   Do not claim Composer or extension compatibility without real
    integration tests.
-   Keep dynamic operations explicit in the IR and runtime.
-   Never compromise native-mode type or memory guarantees to imitate
    dynamic PHP behavior.

------------------------------------------------------------------------

## 10. C++ implementation standards

Use the project's configured C++ standard and formatting conventions.

General rules:

-   Prefer RAII for C++ resource management.
-   Use smart pointers where ownership is shared or unique.
-   Prefer value types and clear ownership.
-   Avoid raw owning pointers in compiler implementation code.
-   Use `const` where appropriate.
-   Avoid unnecessary copies in hot paths, but measure before
    optimizing.
-   Handle errors explicitly; do not silently swallow exceptions.
-   Avoid undefined behavior.
-   Keep headers self-contained.
-   Minimize include dependencies.
-   Use descriptive names.
-   Keep functions focused.
-   Document non-obvious invariants, not obvious syntax.

Do not add C++ exceptions, RTTI, third-party libraries or compiler flags
globally without understanding their effect on the project and its
dependencies.

------------------------------------------------------------------------

## 11. Build and dependency rules

-   Use CMake as the project build system.
-   Document supported compiler, CMake and LLVM versions.
-   Prefer reproducible dependency versions.
-   Do not assume network access is available during builds.
-   Do not require a PHP installation to compile native-mode programs.
-   Keep debug and release configurations working.
-   Do not modify the user's environment or install software without
    authorization.
-   Do not replace project build configuration wholesale when a targeted
    change is sufficient.

For the initial development environment, prioritize Windows x86-64. Add
Linux and macOS targets only when the architecture and toolchain support
them.

------------------------------------------------------------------------

## 12. Testing requirements

Every feature change must include appropriate tests.

### Unit tests

-   Lexer tokenization.
-   Parser output and error handling.
-   Scope and symbol resolution.
-   Type checking.
-   IR construction and verification.
-   Optimizer transformations.
-   Runtime functions.

### Integration tests

-   Compile a `.sph` source file.
-   Link the generated output.
-   Execute the output.
-   Compare stdout, stderr and exit code.
-   Test invalid programs and expected diagnostics.

### Regression tests

For every fixed bug:

1.  Add a test that reproduces the bug.
2.  Verify that it fails before the fix when practical.
3.  Implement the fix.
4.  Verify the test passes.
5.  Run related tests.

Never delete or weaken a failing test merely to make the test suite
green. If a test is obsolete, explain why and replace it with a test for
the intended behavior.

### SPH 0.1 acceptance test

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

Expected executable output:

``` text
300
```

The generated executable must run without PHP installed.

------------------------------------------------------------------------

## 13. Error handling and diagnostics

Compiler errors should be actionable.

Prefer:

``` text
examples/main.sph:8:19: error: expected i32, found string
    let total: i32 = "hello";
                  ^^^^^^^
```

Diagnostics should:

-   Identify the file and source location.
-   Explain what was expected and what was found.
-   Avoid cascading errors where possible.
-   Distinguish errors, warnings and notes.
-   Never report successful compilation after a failed required stage.

Do not replace useful diagnostics with generic messages such as
`Compilation failed`.

------------------------------------------------------------------------

## 14. Security and robustness

Treat source files, package metadata and external inputs as untrusted.

-   Avoid unsafe deserialization.
-   Validate array indices and sizes.
-   Check integer conversions and allocation arithmetic.
-   Avoid command injection in linker invocation.
-   Do not construct shell commands from untrusted source text.
-   Handle malformed source files without crashing.
-   Avoid writing outside the build directory unless explicitly
    requested.
-   Do not run generated executables or arbitrary project scripts
    without authorization when the environment or task requires user
    approval.
-   Keep FFI and unsafe operations visibly documented.

------------------------------------------------------------------------

## 15. Agent workflow for every task

Follow this sequence:

### Step 1 --- Understand

-   Restate the requested engineering outcome internally.
-   Inspect relevant code and documentation.
-   Identify current implementation and dependencies.
-   Identify what is out of scope.

### Step 2 --- Plan

-   Choose the smallest coherent implementation.
-   Identify affected modules and public interfaces.
-   Identify tests.
-   Note compatibility and migration risks.

### Step 3 --- Implement

-   Make focused changes.
-   Follow existing conventions.
-   Avoid unrelated refactoring.
-   Keep the build in a coherent state.
-   Add documentation for user-visible behavior.

### Step 4 --- Verify

Run the most relevant available checks:

1.  Configure/build.
2.  Unit tests.
3.  Integration tests.
4.  Formatting or static analysis if configured.
5.  A minimal real `.sph` compile-and-run test when relevant.

If a check cannot run, state why. Never invent test output or claim a
command succeeded without executing it and observing the result.

### Step 5 --- Report

At completion, report:

-   What changed.
-   Files created or modified.
-   Important design decisions.
-   Commands actually run.
-   Tests that passed.
-   Tests that failed or could not run.
-   Known limitations.
-   The next sensible task.

Do not claim that planned features are implemented. Separate
implemented, partial, planned and unsupported behavior.

------------------------------------------------------------------------

## 16. Scope control and prioritization

Use this priority order unless the user explicitly changes it:

1.  Specification and grammar.
2.  Source management and diagnostics.
3.  Lexer.
4.  Parser and AST.
5.  Semantic analysis.
6.  Static type checker.
7.  Typed IR and verifier.
8.  LLVM backend.
9.  Linker and `sphc` CLI.
10. Native structs and additional language features.
11. Pointers, FFI and systems programming.
12. Ownership and managed memory.
13. Runtime and standard library.
14. PHP compatibility.
15. Advanced optimization and ecosystem tooling.

Do not start PHP compatibility before the native compiler foundation is
reliable. Do not implement every memory model before the first
executable works.

For a solo developer and a modest development machine, prefer
incremental builds, limited parallelism and small test targets. Avoid
unnecessary heavyweight dependencies.

------------------------------------------------------------------------

## 17. Documentation rules

Keep these documents accurate:

-   `README.md`: project overview, current status and build
    instructions.
-   `docs/specification/`: normative language rules.
-   `docs/compiler/`: compiler architecture and implementation details.
-   `docs/standard_library/`: public library behavior.
-   `docs/tutorials/`: user-facing examples.
-   `PLAN.md` or `plan.md`: roadmap and milestones.
-   `AGENT_GUIDE.md`: these contribution and agent rules.

When implementation changes, update the relevant documentation in the
same task. Mark proposals as proposals. Mark unsupported features as
unsupported.

Use code examples that match the current compiler. Label future syntax
as illustrative or planned.

------------------------------------------------------------------------

## 18. Definition of done

A task is done only when:

-   The requested behavior is implemented or the limitation is clearly
    explained.
-   The change follows the language specification.
-   Relevant tests are added or updated.
-   Relevant tests have been run, or inability to run them is disclosed.
-   Documentation is updated where needed.
-   No unrelated files or behavior were changed unnecessarily.
-   The final report accurately distinguishes verified results from
    assumptions.

A directory, class declaration, stub function or TODO is not a completed
feature.

------------------------------------------------------------------------

## 19. First assignment for a new agent

If the repository is new or has no compiler implementation yet:

1.  Inspect the repository and confirm its state.
2.  Read `PLAN.md` and this guide.
3.  Create or refine the SPH 0.1 language specification.
4.  Document the initial grammar and token set.
5.  Set up a minimal CMake C++20 project for `sphc`.
6.  Add a source manager and diagnostic model.
7.  Implement a lexer with unit tests.
8.  Stop at a clean, tested milestone before expanding into the parser.

Do not attempt the full language in one pass.

------------------------------------------------------------------------

## 20. Final rule

**Build what is specified, test what is built, document what is true,
and clearly label everything else as planned.**
