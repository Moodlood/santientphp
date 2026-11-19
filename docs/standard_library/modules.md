# SantientPHP Standard Library Modules

This document details the standard library modules distributed with SantientPHP under `stdlib/`.

---

## Table of Contents

- [1. `std/core.sph` - Core Utilities & Diagnostics](#1-stdcoresph---core-utilities--diagnostics)
- [2. `std/io.sph` - Formatted I/O](#2-stdiosph---formatted-io)
- [3. `std/fs.sph` - Filesystem Operations](#3-stdfssph---filesystem-operations)
- [4. `std/collections.sph` - Collections & Iterators](#4-stdcollectionssph---collections--iterators)
- [5. `std/async.sph` - Concurrency & Event Loop](#5-stdasyncsph---concurrency--event-loop)
- [6. `std/net.sph` - Networking & HTTP](#6-stdnetsph---networking--http)
- [7. `std/crypto.sph` - Cryptography & Randomness](#7-stdcryptosph---cryptography--randomness)
- [8. `std/json.sph` - JSON Serialization & Parsing](#8-stdjsonsph---json-serialization--parsing)
- [9. `std/time.sph` - High-Resolution Time & Durations](#9-stdtimesph---high-resolution-time--durations)
- [10. `std/process.sph` - Process Management & Environment](#10-stdprocesssph---process-management--environment)
- [11. `std/web.sph` - Native Web Application Framework](#11-stdwebsph---native-web-application-framework)
- [12. `std/gui.sph` - Native Win32 Controls & SIMD Canvas](#12-stdguisph---native-win32-controls--simd-canvas)

---

## 1. `std/core.sph` - Core Utilities & Diagnostics

Provides basic runtime diagnostics, assertions, math helper functions, and exit handling.

### Functions

```php
// Assertions & Process Control
fn assert(condition: bool, message: string): void
fn panic_str(message: string): void
fn process_exit(code: i32): void

// Math Helpers (i32)
fn min_i32(a: i32, b: i32): i32
fn max_i32(a: i32, b: i32): i32
fn clamp_i32(val: i32, min_val: i32, max_val: i32): i32
fn abs_i32(val: i32): i32

// Math Helpers (f64)
fn min_f64(a: f64, b: f64): f64
fn max_f64(a: f64, b: f64): f64
fn abs_f64(val: f64): f64
```

### Example

```php
<?php
import "std/core.sph";

fn main(): i32 {
    let x = clamp_i32(150, 0, 100); // returns 100
    assert(x == 100, "x must be clamped to 100");
    return 0;
}
```

---

## 2. `std/io.sph` - Formatted I/O

Provides typed printing functions for strings, integers, floating-point numbers, and booleans.

### Functions

```php
// String I/O
fn print_str(val: string): void
fn println_str(val: string): void

// Integer I/O
fn print_i32(val: i32): void
fn println_i32(val: i32): void
fn print_i64(val: i64): void
fn println_i64(val: i64): void

// Float I/O
fn print_f64(val: f64): void
fn println_f64(val: f64): void

// Boolean I/O
fn print_bool(val: bool): void
fn println_bool(val: bool): void
```

### Example

```php
<?php
import "std/io.sph";

fn main(): i32 {
    println_str("SantientPHP I/O system active.");
    println_i32(42);
    println_bool(true);
    return 0;
}
```

---

## 3. `std/fs.sph` - Filesystem Operations

Provides synchronous filesystem operations, file existence checks, content reading/writing, and file handles.

### Types & Functions

```php
struct FileHandle {
    path: string,
}

fn read_file(path: string): string
fn write_file(path: string, content: string): bool
fn append_file(path: string, content: string): bool
fn file_exists(path: string): bool
fn delete_file(path: string): bool

// FileHandle methods
fn open_file(path: string): FileHandle
fn file_read_all(handle: FileHandle): string
fn file_write_all(handle: FileHandle, content: string): bool
```

### Example

```php
<?php
import "std/fs.sph";
import "std/io.sph";

fn main(): i32 {
    let path = "output.txt";
    write_file(path, "Hello from SantientPHP filesystem!\n");

    if (file_exists(path)) {
        let content = read_file(path);
        println_str(content);
        delete_file(path);
    }
    return 0;
}
```

---

## 4. `std/collections.sph` - Collections & Iterators

Data structures and iteration helpers.

### Types & Functions

```php
struct KeyValuePair {
    key: string,
    value: string,
}

fn make_pair(key: string, value: string): KeyValuePair

struct IntRange {
    start: i32,
    end: i32,
}

fn range(start: i32, end: i32): IntRange
fn range_length(r: IntRange): i32
```

### Example

```php
<?php
import "std/collections.sph";
import "std/io.sph";

fn main(): i32 {
    let pair = make_pair("port", "8080");
    let r = range(0, 10);
    println_i32(range_length(r)); // 10
    return 0;
}
```

---

## 5. `std/async.sph` - Concurrency & Event Loop

Thread sleeping, event loop execution, and cooperative task yielding.

### Functions

```php
fn sleep_ms(ms: i32): void
fn yield_now(): void
fn run_event_loop(): void
```

### Example

```php
<?php
import "std/async.sph";

fn main(): i32 {
    sleep_ms(50);
    yield_now();
    return 0;
}
```

---

## 6. `std/net.sph` - Networking & HTTP

Network client operations backed by the native WinHTTP / POSIX networking runtime.

### Functions

```php
fn http_get(url: string): string
```

### Example

```php
<?php
import "std/net.sph";
import "std/io.sph";

fn main(): i32 {
    let response = http_get("https://httpbin.org/get");
    println_str(response);
    return 0;
}
```

---

## 7. `std/crypto.sph` - Cryptography & Randomness

Cryptographic hashing algorithms and hardware entropy-backed random number generators.

### Functions

```php
fn crypto_sha256(input: string): string
fn crypto_random_u32(): u32
fn crypto_random_range(low: i32, high: i32): i32
```

### Example

```php
<?php
import "std/crypto.sph";
import "std/io.sph";

fn main(): i32 {
    let hash = crypto_sha256("santientphp");
    println_str(hash);

    let roll = crypto_random_range(1, 7); // [1, 6]
    println_i32(roll);
    return 0;
}
```

---

## 8. `std/json.sph` - JSON Serialization & Parsing

Validation and typed key-value stringifying for JSON objects and arrays.

### Functions

```php
fn json_validate(json: string): bool
fn json_escape(s: string): string
fn json_stringify_pair(key: string, val: string): string
fn json_stringify_pair_i32(key: string, val: i32): string
fn json_stringify_pair_bool(key: string, val: bool): string
fn json_wrap_object(content: string): string
fn json_wrap_array(content: string): string
```

### Example

```php
<?php
import "std/json.sph";
import "std/io.sph";

fn main(): i32 {
    let pair1 = json_stringify_pair("name", "SantientPHP");
    let pair2 = json_stringify_pair_i32("version", 1);
    let payload = json_wrap_object(pair1 . "," . pair2);

    if (json_validate(payload)) {
        println_str(payload);
    }
    return 0;
}
```

---

## 9. `std/time.sph` - High-Resolution Time & Durations

Nanosecond-capable microsecond clock timers and duration conversions.

### Types & Functions

```php
struct Instant {
    micros: i64,
}

struct Duration {
    millis: i64,
}

fn instant_now(): Instant
fn instant_elapsed_ms(start: Instant): i64
fn instant_elapsed_micros(start: Instant): i64
fn duration_from_millis(ms: i64): Duration
fn duration_from_secs(s: i64): Duration
```

### Example

```php
<?php
import "std/time.sph";
import "std/async.sph";
import "std/io.sph";

fn main(): i32 {
    let t0 = instant_now();
    sleep_ms(25);
    let elapsed = instant_elapsed_ms(t0);

    println_str("Elapsed ms:");
    println_i64(elapsed);
    return 0;
}
```

---

## 10. `std/process.sph` - Process Management & Environment

Operating system process metadata, exit codes, and environment variable manipulation.

### Functions

```php
fn process_id(): i32
fn process_exit(code: i32): void
fn env_get(name: string): string
fn env_set(name: string, value: string): bool
```

### Example

```php
<?php
import "std/process.sph";
import "std/io.sph";

fn main(): i32 {
    let pid = process_id();
    println_str("Current PID:");
    println_i32(pid);

    env_set("SPH_ENV", "production");
    println_str(env_get("SPH_ENV"));
    return 0;
}
```

---

## 11. `std/web.sph` - Native Web Application Framework

High-performance native web application router, request/response models, and embedded server engine.

### Types & Functions

```php
struct Request {
    method: string,
    path: string,
    body: string,
}

struct Response {
    status_code: i32,
    content_type: string,
    body: string,
}

fn request_create(method: string, path: string, body: string): Request
fn response_ok(body: string): Response
fn response_json(json_body: string): Response
```

### Example

```php
<?php
import "std/web.sph";
import "std/io.sph";

fn main(): i32 {
    let req = request_create("GET", "/api/v1/health", "");
    let res = response_json("{\"status\":\"healthy\"}");

    println_str(res.content_type);
    println_str(res.body);
    return 0;
}
```

---

## 12. `std/gui.sph` - Native Win32 Controls & SIMD Canvas

Direct hardware-accelerated desktop GUI subsystem with Win32 controls (buttons, textboxes, progress bars, dialogs) and a 32-bit SIMD RGBA canvas.

### Types & Functions

```php
struct Window {
    hwnd: *void,
    title: string,
    width: i32,
    height: i32,
}

// Window lifecycle
fn window_create(title: string, width: i32, height: i32): Window
fn window_show(win: Window): void
fn window_hide(win: Window): void
fn window_set_title(win: Window, title: string): void
fn window_is_open(win: Window): bool
fn window_close(win: Window): void

// Native controls
fn button_create(parent: Window, text: string, x: i32, y: i32, w: i32, h: i32, id: i32): *void
fn label_create(parent: Window, text: string, x: i32, y: i32, w: i32, h: i32, id: i32): *void
fn textbox_create(parent: Window, text: string, x: i32, y: i32, w: i32, h: i32, id: i32, multiline: bool): *void
fn progressbar_create(parent: Window, x: i32, y: i32, w: i32, h: i32, id: i32): *void

// SIMD 32-bit RGBA Canvas
fn canvas_create(win: Window, width: i32, height: i32): *void
fn canvas_clear(canvas: *void, color: i32): void
fn canvas_fill_rect(canvas: *void, x: i32, y: i32, w: i32, h: i32, color: i32): void
fn canvas_present(canvas: *void): void

// Event pump & Message dialogs
fn gui_run_event_loop(): void
fn gui_pump_events(): i32
fn gui_quit(): void
fn gui_messagebox(parent: Window, text: string, title: string, utype: i32): i32
```

### Example

```php
<?php
import "std/gui.sph";

fn main(): i32 {
    let win = window_create("SantientPHP App", 640, 480);
    button_create(win, "Click Me", 20, 20, 100, 32, 101);
    window_show(win);

    gui_run_event_loop();
    return 0;
}
```

