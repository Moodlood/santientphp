# Getting Started with SantientPHP

Welcome to SantientPHP! This guide walks you through setting up a project, writing native and dynamic code, using hardware SIMD acceleration, formatting code, and generating documentation.

---

## 1. Verify Your Installation

Before beginning, make sure `sphc` is available and run the system diagnostics:

```powershell
sphc doctor
```

If any core toolchains are missing, `sphc doctor` will point out what is needed.

---

## 2. Create a New Project

Use the built-in package manager to scaffold a new project:

```powershell
sphc new my_first_app
cd my_first_app
```

This creates a standard project directory:
```text
my_first_app/
├── sph.toml          # Project configuration manifest
├── src/
│   └── main.sph      # Application entrypoint
├── .gitignore
└── README.md
```

Inspect `sph.toml`:
```toml
[project]
name = "my_first_app"
version = "0.1.0"
entry = "src/main.sph"

[compiler]
language_version = "0.1"

[target]
triple = "x86_64-windows"

[build]
mode = "release"
optimization = "2"
```

---

## 3. Write Your First Program

Edit `src/main.sph`:

```php
<?php

struct Particle {
    x: i32,
    y: i32,
    mass: i32,
}

fn apply_gravity(p: Particle, g: i32): i32 {
    return p.mass * g;
}

fn main(): i32 {
    let p = Particle { x: 10, y: 20, mass: 5 };
    let weight = apply_gravity(p, 10);

    println("Particle weight:");
    println(weight);
    return 0;
}
```

---

## 4. Compile and Run

You can run the program directly:

```powershell
sphc run
```

Or build an optimized native executable:

```powershell
sphc build -o app.exe
.\app.exe
```

---

## 5. Add 128-bit SIMD Vector Math

SantientPHP allows you to harness hardware vector instructions effortlessly:

```php
<?php

fn calculate_physics(): void {
    let v1 = f32x4(1.0, 2.0, 3.0, 4.0);
    let v2 = f32x4(10.0, 20.0, 30.0, 40.0);

    // Hardware SSE/AVX vector addition in a single CPU cycle
    let result = v1 + v2;
    println(result); // f32x4(11.0, 22.0, 33.0, 44.0)
}
```

---

## 6. Systems Programming & Native Interop

SantientPHP provides bare-metal C FFI, C-compatible structs and unions, typed raw pointers, and null-byte safe binary buffers:

```php
<?php
import "std/buffer.sph";
import "std/thread.sph";

#[repr(C)]
struct Point {
    x: i32,
    y: i32,
}

fn worker(arg: *u8): *u8 {
    let buf = buffer_alloc(1024);
    buffer_set_u8(buf, 0, 0x42 as u8);
    buffer_free(buf);
    unsafe { return 0 as *u8; }
}

fn main(): i32 {
    let m = mutex_create();
    mutex_lock(m);
    unsafe {
        let t = thread_spawn(&worker as *u8, 0 as *u8);
        mutex_unlock(m);
        thread_join(t);
    }
    mutex_destroy(m);
    return 0;
}
```

Compile directly with custom native libraries:
```powershell
sphc build src/main.sph -luser32 -lgdi32 -lws2_32 -o app.exe
```

---

## 7. Code Formatting & Documentation

### Format Source Code
Ensure consistent indentation and spacing:

```powershell
sphc fmt src/main.sph
```

In pre-commit checks or CI pipelines:
```powershell
sphc fmt src/main.sph --check
```

### Generate API Documentation
SantientPHP can generate standalone, search-enabled documentation from docblocks:

```powershell
sphc doc src/main.sph -o docs/api --format=html --title="My First App Docs"
```

Open `docs/api/index.html` in your browser to explore interactive symbol docs and live filtering.

---

## 8. Working in Visual Studio Code

1. Install the official SantientPHP extension:
   ```powershell
   code --install-extension editors\vscode\santientphp-0.1.0.vsix
   ```
2. Open your project folder in VS Code:
   ```powershell
   code .
   ```
3. Enjoy:
   - Full `.sph` syntax highlighting and file iconography.
   - Real-time compiler error squiggles from `sphc lsp`.
   - Hover type signatures and docstrings.
   - Autocompletion for keywords, native types, and PHP functions.

