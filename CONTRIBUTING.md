# Contributing to SantientPHP

Thank you for your interest in contributing to **SantientPHP (SPH)**! SantientPHP is an independent, native compiled language combining the familiarity of PHP with bare-metal speed and systems capabilities.

---

## 1. Development Prerequisites

To build and test the compiler, ensure you have:
- **C++20 capable compiler**: GCC 14+, Clang 17+, or MSVC 2022
- **C99/C11 compiler**: for the runtime libraries
- **CMake**: version 3.20 or newer
- **Ninja**: recommended build system
- **Git**: for version control

---

## 2. Building the Project

Configure and compile in Release mode:

```powershell
# Generate build configuration
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile all targets (compiler, runtime libraries, test suite)
ninja -C build
```

The resulting binaries will be placed in:
- `build/bin/sphc.exe` - SantientPHP compiler CLI
- `build/bin/sph_tests.exe` - Unit and integration test runner
- `build/lib/` - Precompiled runtime libraries (`libsph_runtime.a`, `libsph_core.a`, etc.)

---

## 3. Running the Test Suite

Before making any changes or submitting pull requests, run the test suite:

```powershell
.\build\bin\sph_tests.exe
```

All tests must pass (100% pass rate). When adding new language features, AST nodes, or runtime functions, include corresponding test cases under `tests/`.

---

## 4. Codebase Organization

- `compiler/` - Compiler frontend (lexer, parser, AST, semantic analyzer, type checker), SPHIR intermediate representation, optimization passes, and code generators (x86_64 assembly and LLVM emitter).
- `runtime/` - Native C runtime:
  - `runtime/core/`: memory management, allocators, RC, arenas, strings, vectors, SIMD intrinsics.
  - `runtime/async/`: threading, atomics, fibers, event loop.
  - `runtime/compat/`: dynamic values (`sph_value`), PHP associative arrays, PDO SQLite, cURL.
  - `runtime/pal.h`: cross-platform abstraction layer.
- `stdlib/` - Normative standard library written in SantientPHP (`.sph`).
- `editors/vscode/` - Official VS Code extension and LSP integration.
- `docs/` - Formal language specification, architecture docs, and tutorials.
- `examples/` - Reference programs showcasing static, systems, and dynamic modes.

---

## 5. Coding Standards

- **Compiler (C++20)**: Modern idioms, RAII, smart pointers (`std::unique_ptr`, `std::shared_ptr`), descriptive names, no raw owning pointers.
- **Runtime (C99/C11)**: Strict prefixing (`sph_` or `SPH_`), clear memory ownership rules, error code propagation.
- **Standard Library (`.sph`)**: Follow the official formatting conventions using `sphc fmt`.

---

## 6. Commit Message Guidelines

We follow Conventional Commits:
- `feat:` for new language features or compiler capabilities
- `fix:` for bug fixes
- `docs:` for documentation updates
- `test:` for adding or updating tests
- `refactor:` for code restructuring without behavior changes
- `chore:` for build system, CI, or tooling updates

---

## 7. Submitting Changes

1. Fork or branch from `main`.
2. Implement your feature or fix with tests.
3. Verify the build (`ninja -C build`) and tests (`sph_tests.exe`).
4. Commit your changes with clear messages.
5. Submit a Pull Request describing your changes and verification steps.
