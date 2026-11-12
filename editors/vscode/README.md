# SantientPHP for Visual Studio Code

Official Visual Studio Code extension for **SantientPHP (SPH)** — combining the developer ergonomics of PHP with high-performance native systems programming, coroutines, and SIMD hardware acceleration.

## Features

- **Language Server Protocol (`sphc lsp`)**:
  - **Live Diagnostics**: Real-time syntax and semantic analysis with squiggly error underlines in the editor and Problems panel.
  - **Hover Information**: Instant type signatures, doc comments, and parameter specifications for functions, structs, classes, and language keywords.
  - **Autocompletion**: Context-aware completion across keywords, types (`i32`, `dynamic`, `f32x4`, `AtomicI32`), built-in PHP functions, and in-scope symbols.
  - **Go to Definition (F12)**: Jump directly to symbol definitions in the current workspace.
  - **Document Outline & Breadcrumbs**: Hierarchical symbol outlines for functions, structs, classes, interfaces, and enums.
- **Rich Syntax Highlighting**:
  - Full TextMate grammar for `.sph` source files supporting PHP opening tags, static/dynamic types, PHP variables (`$var`), docblocks, and native operators.
- **Integrated Tooling**:
  - **Doctor Diagnostics (`SantientPHP: Run Doctor Diagnostics`)**: Check the health of your local compilers, linkers, debuggers, and runtime libraries.
  - **Build & Run (`SantientPHP: Build Active File / Project`, `SantientPHP: Run Active File`)**: Single-click compilation and execution in the integrated terminal.
  - **Code Formatting (`SantientPHP: Format File with sphc fmt`)**: Standardized indentation, operator spacing, and comment alignment.

## Requirements

The SantientPHP compiler binary `sphc` (or `sphc.exe` on Windows).
The extension automatically looks for `sphc` in:
1. Workspace `build/bin/sphc.exe`
2. Workspace `bin/sphc.exe`
3. System `PATH`
4. Custom path configured in `santientphp.compilerPath`

## Extension Settings

* `santientphp.compilerPath`: Absolute or relative path to the `sphc` executable.
* `santientphp.trace.server`: Traces communication between VS Code and the Language Server (`off`, `messages`, `verbose`).
* `santientphp.format.onSave`: Automatically format files on save.

