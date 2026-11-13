# SantientPHP (SPH)

**SantientPHP** is an independent, compiled programming language that combines the familiar syntax and developer ergonomics of PHP with the bare-metal performance, memory safety, and concurrency capabilities of a modern systems language.

It compiles directly to native machine code (`.exe` on Windows, native binaries on Linux and macOS) without requiring a PHP runtime or interpreter.

---

## Key Features

- **Triple-Mode Programming Model**:
  - **Static Mode**: Deterministic static typing with value structs, enums, OOP classes, interfaces, and generics.
  - **Systems Mode**: Raw pointers, memory layout controls, arenas, and direct C FFI integration.
  - **Dynamic PHP Compatibility Mode**: First-class `dynamic` values (`sph_value`), associative arrays (`sph_array`), loose coercions, PHP object models, WinHTTP cURL, and SQLite PDO.
- **Hardware Concurrency & Async**:
  - Native OS threading (`Thread::spawn`, `Thread::sleep_ms`) and hardware atomic primitives (`AtomicI32`, `AtomicI64`, `AtomicBool`).
  - Cooperative coroutines (`async fn`, `await`, `Promise<T>`) backed by an integrated event loop runtime.
- **128-bit SIMD Acceleration**:
  - Hardware vector types (`f32x4`, `i32x4`, `f64x2`, `i64x2`) utilizing x86_64 SSE/AVX instruction sets alongside portable scalar fallbacks.
- **Dual Native Code-Generation**:
  - Direct x86_64 assembly code generator (`X86_64CodeGen`) with DWARF line tables and GNU Call Frame Information (CFI) for GDB debugging.
  - LLVM IR emitter (`LLVMEmitter`) with target data layout and triple normalization.
- **Custom Typed IR & Optimization Pipeline**:
  - Verifiable intermediate representation (`SPHIR`).
  - Multi-pass optimizer: constant folding, algebraic simplifications, common subexpression elimination (CSE), dead-code elimination, control-flow optimization, and leaf function inlining.
- **Full Developer Ecosystem**:
  - **Code Formatter (`sphc fmt`)**: Indentation, operator alignment, and comment preservation.
  - **Documentation Generator (`sphc doc`)**: Extracts docblocks to GitHub Markdown or responsive, search-enabled standalone HTML.
  - **Package & Project Manager (`sphc new`, `sphc init`, `sphc pkg`)**: Project scaffolding and dependency tracking via `sph.toml`.
  - **Language Server Protocol (`sphc lsp`)**: Standalone JSON-RPC 2.0 LSP 3.17 engine providing real-time diagnostics, hover signatures, autocompletion, go-to-definition, and symbol outlines.
  - **Health Diagnostics (`sphc doctor`)**: Local toolchain and system library verification.
  - **Visual Studio Code Extension**: Syntax highlighting, `.sph` file iconography, and integrated LSP client.

---

## Quick Start

### 1. Build the Compiler

**Prerequisites**:
- C++20 compiler (GCC 14+ / Clang 17+ on MinGW-w64 or Linux)
- CMake 3.20+
- Ninja build system

```powershell
# Clone and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# Verify full test suite (252 tests)
.\build\bin\sph_tests.exe
```

### 2. Install to Windows (Drive C & Program Files)

Install SantientPHP as a system-wide compiler just like standard PHP:

- **Option A (One-Click Batch)**: Double-click or run `install.bat` (prompts for UAC Administrator elevation automatically).
- **Option B (PowerShell)**: Run `powershell -ExecutionPolicy Bypass -File .\install.ps1` in an elevated terminal.

This installs:
- Primary location: `C:\Program Files\SantientPHP\`
- Drive C alias: `C:\SantientPHP\` (NTFS directory junction)
- Executables & wrappers in `bin/`: `sphc.exe`, `sph.exe`, `santientphp.exe`
- Precompiled runtime in `lib/`: `libsph_runtime.a`
- PHP-like configuration in root: `sph.ini` (`sph.ini-development`, `sph.ini-production`)
- System `PATH` and `SPH_HOME` / `SANTIENTPHP_HOME` environment variables
- Windows Explorer `.sph` file association and context menu actions
- Registered in Windows **Installed Apps / Programs and Features**

To uninstall at any time, run `uninstall.bat` or `uninstall.ps1` (or uninstall from Windows Settings / Control Panel).

### 3. Verify Your Toolchain

```powershell
.\build\bin\sphc.exe doctor
```

```text
SantientPHP Doctor [v0.1.0]
===========================
  [OK] Operating System: Microsoft Windows (x86_64)
  [OK] Host C/C++ Compiler (gcc): gcc 15.2.0
  [OK] Linker (ld): GNU ld 2.45.1
  [OK] Debugger (gdb): GNU gdb 17.1
  [OK] Version Control (git): git version 2.47.1
  [OK] Windows Native Subsystems: kernel32.dll, winhttp.dll, winsqlite3.dll
  [OK] SantientPHP Core Runtime: Precompiled runtime libraries present (libsph_runtime.a)
  [OK] SantientPHP Standard Library (stdlib/): Found at stdlib/
```

### 4. Run Your First Program

Create `hello.sph`:

```php
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

Run immediately:

```powershell
sphc run hello.sph
# Output: 300
```

Or build an optimized standalone executable:

```powershell
sphc build hello.sph -O3 -o hello.exe
.\hello.exe
```

---

## Command-Line Interface (`sphc`)

| Command | Description |
| :--- | :--- |
| `sphc build [file.sph]` | Compiles source file to a native executable or object |
| `sphc run [file.sph]` | Compiles and executes in one step |
| `sphc check [file.sph]` | Validates syntax and semantic types without code generation |
| `sphc test [file.sph]` | Executes unit tests or runs the complete test suite |
| `sphc clean` | Cleans local compiler build artifacts and intermediate caches |
| `sphc inspect <file.sph>` | Inspects compiler pipeline stages (`--tokens`, `--ast`, `--ir`, `--asm`, `--llvm`) |
| `sphc fmt [file.sph]` | Formats source code in-place (`--check`, `--stdout`) |
| `sphc doc [file.sph]` | Generates API documentation (`--format=html\|md`, `-o <path>`) |
| `sphc lsp` | Launches Language Server Protocol engine over standard I/O |
| `sphc doctor` | Diagnoses host toolchains and system libraries |
| `sphc new <name>` | Scaffolds a new SantientPHP project structure |
| `sphc init` | Initializes `sph.toml` manifest in the current directory |
| `sphc pkg add <name>` | Adds a path (`--path=...`) or git (`--git=...`) dependency |
| `sphc pkg list` | Lists registered dependencies in `sph.toml` |
| `sphc targets` | Displays all supported host and cross-compilation target triples |

---

## Language Highlights

### 1. Value Structs & Enums
```php
<?php
struct Vector3 {
    x: i32,
    y: i32,
    z: i32,
}

enum State {
    Idle,
    Running,
    Finished,
}
```

### 2. Hardware SIMD Vectors (128-bit)
```php
<?php
let v1 = f32x4(1.0, 2.0, 3.0, 4.0);
let v2 = f32x4(5.0, 6.0, 7.0, 8.0);
let sum = v1 + v2;
let dot = sum.dot(sum);
```

### 3. Cooperative Async Coroutines
```php
<?php
async fn compute_async(val: i32): Promise<i32> {
    EventLoop::yield_now();
    return val * 2;
}
```

### 4. Deterministic Resource Cleanup (`defer`)
```php
<?php
fn process_buffer(): i32 {
    mut buffer = allocate_buffer(1024);
    defer free_buffer(buffer);

    return compute(buffer);
}
```

### 5. Dynamic PHP Compatibility & Boundary Conversions
```php
<?php
$data = ["language" => "SantientPHP", "speed" => "native"];
$data["version"] = 1.0;
echo "Name: " . $data["language"] . "\n";
var_dump($data);

// Seamless, checked boundary casts and unboxing intrinsics:
$count = 42;
let native_count: i32 = intval($count);
let casted: i32 = ($count as i32);
```

### 6. Native Standard Library (`stdlib/`)
```php
<?php
import "std/core.sph";
import "std/io.sph";
import "std/collections.sph";

fn main(): i32 {
    let r = range(0, 10);
    println_i32(range_length(r)); // 10
    println_str("Standard library ready.");
    return 0;
}
```

---

## Editor Integration (VS Code)

The official Visual Studio Code extension is located in [editors/vscode](editors/vscode):
- Provides syntax highlighting, auto-closing pairs, and `.sph` file iconography.
- Automatically connects to `sphc lsp` for live diagnostics, autocompletion, hover docs, and symbol outlines.
- One-click commands for `sphc build`, `sphc run`, `sphc fmt`, and `sphc doctor`.

Install locally:
```powershell
code --install-extension editors\vscode\santientphp-0.1.0.vsix
```

---

## Project Structure

```text
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
│   ├── optimizer/         # Multi-pass IR optimization pipeline
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
├── docs/                  # Language specification (1.0 Freeze), architecture, tutorials
├── examples/              # Positive language examples and feature demonstrations
├── tests/                 # 252 automated unit and integration tests (100% pass)
├── tools/sphc/            # Main compiler CLI driver
├── CMakeLists.txt         # Root build configuration
└── sph.toml               # Manifest configuration
```

---

## License

This project is licensed under the MIT License.

