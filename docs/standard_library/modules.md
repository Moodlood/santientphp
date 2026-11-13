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

