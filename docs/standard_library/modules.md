# SantientPHP Standard Library Modules

This document details the standard library modules distributed with SantientPHP under `stdlib/`.

---

## Table of Contents

- [1. `std/core.sph` - Core Utilities & Diagnostics](#1-stdcoresph---core-utilities--diagnostics)
- [2. `std/buffer.sph` - Binary Buffer Manipulation (`ByteBuffer`)](#2-stdbuffersph---binary-buffer-manipulation-bytebuffer)
- [3. `std/thread.sph` - Multi-Threading & Synchronization](#3-stdthreadsph---multi-threading--synchronization)
- [4. `std/io.sph` - Formatted I/O](#4-stdiosph---formatted-io)
- [5. `std/fs.sph` - Filesystem Operations](#5-stdfssph---filesystem-operations)
- [6. `std/collections.sph` - Collections & Iterators](#6-stdcollectionssph---collections--iterators)
- [7. `std/async.sph` - Concurrency & Event Loop](#7-stdasyncsph---concurrency--event-loop)
- [8. `std/net.sph` - Networking, HTTP & WebSockets](#8-stdnetsph---networking-http--websockets)
- [9. `std/crypto.sph` - Cryptography & Randomness](#9-stdcryptosph---cryptography--randomness)
- [10. `std/json.sph` - JSON Serialization & Parsing](#10-stdjsonsph---json-serialization--parsing)
- [11. `std/time.sph` - High-Resolution Time & Durations](#11-stdtimesph---high-resolution-time--durations)
- [12. `std/process.sph` - Process Management & Environment](#12-stdprocesssph---process-management--environment)
- [13. `std/web.sph` - Native Web Application Framework & Embedded Server](#13-stdwebsph---native-web-application-framework--embedded-server)
- [14. `std/gui.sph` - Native Win32 Controls & SIMD Canvas](#14-stdguisph---native-win32-controls--simd-canvas)

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

## 2. `std/buffer.sph` - Binary Buffer Manipulation (`ByteBuffer`)

Provides zero-overhead, null-byte safe binary buffers for systems programming, file format decoders/encoders, network packet assembly, and device drivers. Unlike strings, `ByteBuffer` handles arbitrary `0x00` null bytes without truncation.

### Functions & Methods

```php
// Functional API
fn buffer_alloc(capacity: i64): *u8
fn buffer_from_raw(ptr: *u8, len: i64): *u8
fn buffer_len(buf: *u8): i64
fn buffer_cap(buf: *u8): i64
fn buffer_ptr(buf: *u8): *u8
fn buffer_get_u8(buf: *u8, offset: i64): u8
fn buffer_set_u8(buf: *u8, offset: i64, val: u8): void
fn buffer_get_u16_le(buf: *u8, offset: i64): u16
fn buffer_set_u16_le(buf: *u8, offset: i64, val: u16): void
fn buffer_get_u32_le(buf: *u8, offset: i64): u32
fn buffer_set_u32_le(buf: *u8, offset: i64, val: u32): void
fn buffer_append(buf: *u8, src: *u8, len: i64): void
fn buffer_write(buf: *u8, offset: i64, src: *u8, count: i64): void
fn buffer_read(buf: *u8, offset: i64, dst: *u8, count: i64): void
fn buffer_slice(buf: *u8, start: i64, len: i64): *u8
fn buffer_free(buf: *u8): void

// Class Wrapper
class ByteBuffer {
    public function __construct(capacity: i64)
    public static function alloc(capacity: i64): ByteBuffer
    public static function from_raw(ptr: *u8, len: i64): ByteBuffer
    public function len(): i64
    public function cap(): i64
    public function ptr(): *u8
    public function get_u8(offset: i64): u8
    public function set_u8(offset: i64, val: u8): void
    public function get_u16_le(offset: i64): u16
    public function set_u16_le(offset: i64, val: u16): void
    public function get_u32_le(offset: i64): u32
    public function set_u32_le(offset: i64, val: u32): void
    public function append(src: *u8, len: i64): void
    public function write(offset: i64, src: *u8, count: i64): void
    public function slice(start: i64, len: i64): ByteBuffer
    public function free(): void
}
```

### Example

```php
<?php
import "std/buffer.sph";
import "std/io.sph";

fn main(): i32 {
    let buf = buffer_alloc(1024);
    buffer_set_u8(buf, 0, 0x42 as u8); // 'B'
    buffer_set_u8(buf, 1, 0x4D as u8); // 'M'
    buffer_set_u32_le(buf, 2, 1024 as u32);
    
    println_i64(buffer_len(buf));
    buffer_free(buf);
    return 0;
}
```

---

## 3. `std/thread.sph` - Multi-Threading & Synchronization

Provides native operating system threads and recursive mutual exclusion locks (`Mutex`) backed directly by Win32 (`CreateThread`, `CRITICAL_SECTION`) and POSIX (`pthread_create`, `pthread_mutex_t`).

### Functions & Classes

```php
// Functional API
fn thread_spawn(fn_ptr: *u8, arg: *u8): *u8
fn thread_join(handle: *u8): void
fn thread_sleep(ms: i32): void

fn mutex_create(): *u8
fn mutex_lock(mutex: *u8): void
fn mutex_unlock(mutex: *u8): void
fn mutex_destroy(mutex: *u8): void

// Classes
class Thread {
    public static function spawn(fn_ptr: *u8, arg: *u8): Thread
    public function join(): void
    public static function sleep(ms: i32): void
}

class Mutex {
    public function lock(): void
    public function unlock(): void
    public function destroy(): void
}
```

### Example

```php
<?php
import "std/thread.sph";
import "std/io.sph";

fn worker(arg: *u8): *u8 {
    println_str("Worker thread executing");
    thread_sleep(10);
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

---

## 4. `std/io.sph` - Formatted I/O

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

## 5. `std/fs.sph` - Filesystem Operations

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

## 6. `std/collections.sph` - Collections & Iterators

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

## 7. `std/async.sph` - Concurrency & Event Loop

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

## 8. `std/net.sph` - Networking, HTTP & WebSockets

Network operations backed by native WinHTTP, Winsock2, and POSIX networking runtimes. Includes zero-dependency RFC 6455 WebSocket client integration with automatic masking, ping/pong heartbeats, and SHA-1 Sec-WebSocket handshake.

### Types & Functions

```php
// HTTP Primitives
struct HttpResponse {
    status_code: i32,
    headers: string,
    body: string,
}

fn http_get(url: string): string
fn server_listen(port: i32): i32
fn server_close(fd: i32): void

// WebSocket Functional Primitives
fn ws_connect(host: string, port: i32, path: string): *u8
fn ws_is_open(handle: *u8): bool
fn ws_send_text(handle: *u8, message: string): i32
fn ws_send_ping(handle: *u8): i32
fn ws_recv(handle: *u8, timeout_ms: i32): i32
fn ws_get_message(handle: *u8): string
fn ws_close(handle: *u8): void

// WebSocket Client Class
class WebSocketClient {
    public function __construct(host: string, port: i32, path: string)
    public function isOpen(): bool
    public function send(message: string): bool
    public function ping(): bool
    public function receive(timeout_ms: i32): string
    public function close(): void
}
```

### Examples

**HTTP GET:**
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

**WebSocket Client:**
```php
<?php
import "std/net.sph";
import "std/io.sph";

fn main(): i32 {
    let ws = ws_connect("127.0.0.1", 4000, "/control");
    if (ws_is_open(ws)) {
        ws_send_text(ws, "{\"action\":\"ping\"}");
        let op = ws_recv(ws, 1000);
        if (op == 1) { // 1 = Opcode Text
            let msg = ws_get_message(ws);
            println_str(msg);
        }
        ws_close(ws);
    }
    return 0;
}
```

---

## 9. `std/crypto.sph` - Cryptography & Randomness

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

## 10. `std/json.sph` - JSON Serialization & Parsing

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

## 11. `std/time.sph` - High-Resolution Time & Durations

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

## 12. `std/process.sph` - Process Management & Environment

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

## 13. `std/web.sph` - Native Web Application Framework & Embedded Server

High-performance native web application router, request/response models, and embedded non-blocking HTTP server engine with `WSAPoll` / POSIX `poll` multiplexing.

### Types & Functions

```php
// Embedded Server Primitives
fn server_accept(server_fd: i32, timeout_ms: i32): i32
fn server_read_request(client_fd: i32, timeout_ms: i32): string
fn server_write_response(client_fd: i32, status: i32, content_type: string, body: string): i32
fn server_write_raw(client_fd: i32, data: *u8, len: i64): i32
fn server_poll(fd: i32, timeout_ms: i32): bool

// Request & Response Types
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

// Web Application Classes
class WebResponse {
    public function status($code): WebResponse
    public function json($data): WebResponse
    public function text($text): WebResponse
    public function send($text): WebResponse
}

class WebApp {
    public function get($path, $handler): void
    public function post($path, $handler): void
    public function use($mw): void
    public function listen($port): i32
    public function serve_once($port, $timeout_ms): i32
}
```

### Examples

**Standalone Server Loop:**
```php
<?php
import "std/web.sph";
import "std/io.sph";

fn main(): i32 {
    let s = server_listen(8080);
    if (s >= 0) {
        println_str("Listening on port 8080...");
        let client = server_accept(s, 5000);
        if (client >= 0) {
            let req = server_read_request(client, 1000);
            server_write_response(client, 200, "application/json", "{\"status\":\"ok\"}");
            server_close(client);
        }
        server_close(s);
    }
    return 0;
}
```

---

## 14. `std/gui.sph` - Native Win32 Controls & SIMD Canvas

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

