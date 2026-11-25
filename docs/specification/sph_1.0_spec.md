# SantientPHP (SPH) 1.0 Formal Language Specification & ABI Freeze

**Document Status:** Normative Reference Specification (Milestone 4.3 Baseline)  
**Specification Version:** 1.0.0-rc1 (Language Specification 1.0 Freeze)  
**Compiler Implementation:** `sphc` v0.2.2  
**Target Architectures:** Windows x86_64 (`x86_64-pc-windows-gnu`), Linux x86_64 (`x86_64-unknown-linux-gnu`), macOS ARM64 (`aarch64-apple-darwin`)  
**Standard Library Version:** 1.0.0  
**Compiler:** `sphc`  
**File Extension:** `.sph`  

---

## 1. Introduction & Conformance Targets

SantientPHP (SPH) is a dual-nature systems and application programming language designed to unite the ergonomic expressiveness of PHP with the deterministic safety, low overhead, and direct compilation of modern systems languages (C++, Rust, Zig).

> [!NOTE]
> **Specification Freeze vs. Implementation Release Version**: The SantientPHP grammar, syntax rules, and type system are frozen at **Language Specification 1.0** to guarantee semantic stability and code portability. The reference compiler implementation (`sphc`) and accompanying toolchain are versioned at release **v0.2.2**, reflecting ongoing development on backend optimization passes, platform backends, and developer tooling.

### 1.1 Conformance Classes
A conforming SantientPHP implementation consists of:
1. **The Compiler (`sphc`)**: An executable toolchain capable of lexing, parsing, analyzing, optimizing (SPHIR), and emitting valid native machine code (either directly via `X86_64CodeGen` or via LLVM IR).
2. **The Modular Runtime (`libsph_runtime.a`)**: A C runtime partitioned into `libsph_core.a` (memory, arenas, strings, formatting, panics), `libsph_async.a` (stackful fibers, event loop, timer callbacks), `libsph_net.a` (TCP, HTTP, WebSocket, epoll), `libsph_compat.a` (WinHTTP/cURL networking, SQLite/PDO database engine), and `libsph_gui.a` (Win32 and X11/GLX native GUI subsystems).
3. **The Standard Library (`stdlib/`)**: Normative source packages exposing fundamental primitives, I/O abstractions, collections, and asynchronous fibers.

### 1.2 File Structure & Opening Tag
Every valid SPH source file MUST begin with the opening tag `<?php`. The closing tag `?>` is omitted in pure SPH source files.
```php
<?php
// SantientPHP 1.0 Source File
```

---

## 2. Lexical & Syntactic Grammar (EBNF)

### 2.1 Lexical Grammar

```ebnf
SourceFile      ::= "<?php" TopLevelItem* EOF

Identifier      ::= [a-zA-Z_][a-zA-Z0-9_]*
DynamicVariable ::= "$" [a-zA-Z_][a-zA-Z0-9_]*

IntegerLiteral  ::= [0-9]+ | "0x" [0-9a-fA-F]+ | "0b" [01]+
FloatLiteral    ::= [0-9]+ "." [0-9]+ ([eE][+-]?[0-9]+)?
StringLiteral   ::= '"' DoubleQuoteChar* '"' | "'" SingleQuoteChar* "'"
BooleanLiteral  ::= "true" | "false"
NullLiteral     ::= "null"
```

### 2.2 Syntactic Grammar

```ebnf
TopLevelItem    ::= ImportDecl
                  | AttributeList? FunctionDecl
                  | AttributeList? StructDecl
                  | AttributeList? UnionDecl
                  | AttributeList? EnumDecl
                  | AttributeList? ClassDecl
                  | AttributeList? InterfaceDecl
                  | ExternDecl

ImportDecl      ::= "import" StringLiteral ";"

AttributeList   ::= "#[" Attribute ("," Attribute)* "]"
Attribute       ::= Identifier ("(" AttributeArgs? ")")?

FunctionDecl    ::= "fn" Identifier "(" ParamList? ")" (":" Type)? Block
ExternDecl      ::= "extern" StringLiteral? "fn" Identifier "(" ParamList? ")" (":" Type)? ";"

StructDecl      ::= "struct" Identifier "{" (StructField ("," StructField)* ","?)? "}"
UnionDecl       ::= "union" Identifier "{" (StructField ("," StructField)* ","?)? "}"
StructField     ::= Identifier ":" Type

EnumDecl        ::= "enum" Identifier "{" (EnumVariant ("," EnumVariant)* ","?)? "}"
EnumVariant     ::= Identifier ("(" Type ("," Type)* ")")?

Block           ::= "{" Statement* "}"

Statement       ::= LetStmt
                  | MutStmt
                  | DeferStmt
                  | ReturnStmt
                  | IfStmt
                  | WhileStmt
                  | ForStmt
                  | MatchStmt
                  | ExprStmt
                  | Block

MatchStmt       ::= "match" "("? Expression ")"? "{" MatchArm* "}"
MatchArm        ::= Pattern ("|" Pattern)* ("if" Expression)? "=>" (Block | Expression ("," | ";"))
Pattern         ::= Literal | Identifier | Identifier "::" Identifier ("(" Pattern* ")")? | RangePattern | "_"
RangePattern    ::= Expression ".." Expression
TryPropagate    ::= Expression "?"

LetStmt         ::= "let" Identifier (":" Type)? ("=" Expression)? ";"
MutStmt         ::= "mut" Identifier (":" Type)? ("=" Expression)? ";"
DeferStmt       ::= "defer" (Block | Statement)
ReturnStmt      ::= "return" Expression? ";"
IfStmt          ::= "if" "(" Expression ")" Block ("else" (IfStmt | Block))?
WhileStmt       ::= "while" "(" Expression ")" Block
ForStmt         ::= "for" "(" Identifier "in" Expression ")" Block
ExprStmt        ::= Expression ";"
```

---

## 3. Type System & Memory Model

### 3.1 Scalar Types
| Type | Bit Width | Alignment | Description |
| :--- | :--- | :--- | :--- |
| `i8`, `u8` | 8 bits | 1 byte | Signed / unsigned 8-bit integer |
| `i16`, `u16` | 16 bits | 2 bytes | Signed / unsigned 16-bit integer |
| `i32`, `u32` | 32 bits | 4 bytes | Standard signed / unsigned integer |
| `i64`, `u64` | 64 bits | 8 bytes | 64-bit integer |
| `usize` | 64 bits | 8 bytes | Target pointer-width unsigned integer |
| `f32` | 32 bits | 4 bytes | IEEE 754 single-precision float |
| `f64` | 64 bits | 8 bytes | IEEE 754 double-precision float |
| `bool` | 8 bits | 1 byte | Boolean value (`true` or `false`) |
| `char` | 32 bits | 4 bytes | Unicode scalar code point |
| `void` | 0 bits | - | Absence of value |
| `*T`, `ptr<T>` | 64 bits | 8 bytes | Raw systems pointer (Systems Mode / C FFI) |
| `*const T`, `*mut T` | 64 bits | 8 bytes | Raw const / mut pointer (Systems Mode) |

### 3.2 Compound Types, Unions & `#[repr(C)]`
Struct and union field declarations are comma-delimited:
```php
#[repr(C)]
struct MOUSEINPUT {
    dx: i32,
    dy: i32,
    mouseData: u32,
    dwFlags: u32,
    time: u32,
    dwExtraInfo: usize,
}

#[repr(C)]
union InputPayload {
    mi: MOUSEINPUT,
}

#[repr(C)]
struct INPUT {
    type: u32,
    payload: InputPayload,
}
```
Field layout in memory follows target C ABI alignment rules:
- **Structs**: Natural alignment; field offset aligned to `min(sizeof(field), max_alignment)`. Total struct size padded to multiple of largest member alignment.
- **Unions**: Every union field has byte offset `0`. Total union size is the maximum size across all member types, padded to the largest member alignment.
- **`sizeof(T)` Operator**: Evaluated at compile time to a constant integer equal to the byte footprint of type `T`.
- **Function Pointers & Callbacks**: Applying `&fn_name` produces a raw code pointer (`*u8`) directly compatible with OS callback signatures (e.g. Win32 `WNDPROC`, `LPTHREAD_START_ROUTINE`).
- **Binary Buffers (`ByteBuffer`)**: Zero-overhead null-byte safe dynamic buffer type in `std/buffer.sph` preserving arbitrary `0x00` bytes.

### 3.3 Ownership & Deterministic Cleanup (`defer`)
SantientPHP employs lexical scoping with deterministic stack deallocation and the `defer` keyword.
- A `defer` statement queues an expression or block to execute when the enclosing scope exits.
- Multiple `defer` statements in the same scope execute in **Last-In, First-Out (LIFO)** reverse order.
- Defer blocks execute prior to function returns, loop breaks, and panic unwind paths:
```php
fn process_file(path: string): i32 {
    let handle = open_file(path);
    defer close_file(handle);

    if (is_invalid(handle)) {
        return -1; // defer runs before return
    }
    return read_data(handle); // defer runs before return
}
```

### 3.4 Sum Types & Algebraic Pattern Matching
SantientPHP provides native sum types, error propagation, and pattern matching:
- **`Option<T>`**: Encapsulates an optional value (`Some(T)` or `None`).
- **`Result<T, E>`**: Encapsulates successful computation (`Ok(T)`) or failure (`Err(E)`).
- **Error Propagation Operator (`?`)**: Suffix operator on `Result<T, E>` or `Option<T>` expressions that unwraps the inner value on success or returns early on error.
- **Pattern Matching (`match`)**: Multi-way branching that tests an expression against value literals, closed/open ranges (`1..10`), wildcards (`_`), and boolean guard conditions (`if <cond>`).

---

## 4. The Dual-Mode Hybrid Boundary (Static vs. Dynamic)

SantientPHP strictly distinguishes between unboxed static variables and boxed dynamic variables:

### 4.1 Sigil Distinctions
- **Static Variables** (`let`, `mut`): Unboxed native values stored in CPU registers or on the stack with zero runtime overhead.
- **Dynamic Variables** (`$var`): Boxed values backed by the runtime's 24-byte `sph_value_t` tagged union (`SPH_TYPE_NULL`, `SPH_TYPE_BOOL`, `SPH_TYPE_INT`, `SPH_TYPE_FLOAT`, `SPH_TYPE_STRING`, `SPH_TYPE_ARRAY`, `SPH_TYPE_OBJECT`).

### 4.2 Explicit Boundary Casts & Intrinsics
Casting between static and dynamic types requires explicit unboxing or built-in coercion intrinsics:
```php
let dynVal = $data;
let n: i32 = ($dynVal as i32); // Boundary cast

// Standard coercions
let int_res: i32 = intval($data);
let flt_res: f64 = floatval($data);
let str_res: string = strval($data);
let b_res: bool = boolval($data);
```
If an explicit cast cannot be completed at runtime without truncation or tag mismatch, the runtime executes `sph_panic()` to prevent memory corruption or silent type coercion bugs.

---

## 5. Application Binary Interface (ABI) Specification (1.0 Freeze)

### 5.1 Calling Conventions
All public SantientPHP native functions default to the platform's native C calling convention:

#### Windows x86-64 (`x86_64-pc-windows-gnu`)
1. **Integer/Pointer Arguments**: `RCX`, `RDX`, `R8`, `R9`. Additional parameters passed on the stack right-to-left.
2. **Floating-Point Arguments**: `XMM0`, `XMM1`, `XMM2`, `XMM3`.
3. **Shadow Space**: 32 bytes (4 quadwords) allocated on the caller stack immediately preceding `CALL`.
4. **Stack Alignment**: 16-byte aligned at every function call boundary.
5. **Callee-Saved Registers**: `RBX`, `RBP`, `RDI`, `RSI`, `R12`, `R13`, `R14`, `R15`, `XMM6`–`XMM15`.

#### Linux & macOS x86-64 (`x86_64-unknown-linux-gnu`)
1. **Integer/Pointer Arguments**: `RDI`, `RSI`, `RDX`, `RCX`, `R8`, `R9`.
2. **Floating-Point Arguments**: `XMM0`–`XMM7`.
3. **Stack Alignment**: 16-byte aligned.
4. **Callee-Saved Registers**: `RBX`, `RBP`, `R12`, `R13`, `R14`, `R15`.

### 5.2 Symbol Name Mangling
- Internal functions: `_sph_<module_path>_<fn_name>`
- `extern "C"` functions: Raw unmangled identifier (e.g. `sph_process_exit`, `malloc`, `free`).

---

## 6. Concurrency & Stackful Fiber Execution Model

SantientPHP provides asynchronous concurrency via stackful fibers abstracted through the Platform Abstraction Layer (`runtime/pal.h`).

### 6.1 Fiber Structure
```c
typedef struct sph_fiber {
    void* stack_ptr;
    void* stack_base;
    size_t stack_size;
    void (*entry)(void*);
    void* arg;
    int state;             // 0: READY, 1: RUNNING, 2: SUSPENDED, 3: TERMINATED
    struct sph_fiber* next;
} sph_fiber_t;
```

### 6.2 Fiber Context Switching
- On Windows: Managed via Win32 Fiber APIs (`CreateFiberEx`, `SwitchToFiber`, `DeleteFiber`).
- On POSIX: Managed via `makecontext`/`swapcontext` or assembly context switch.
- Yielding: Invoking `yield_now()` or `sleep_ms(ms)` suspends the current fiber, returns execution to the runtime event loop, and resumes when ready.

---

## 7. Modular Runtime Architecture

The SantientPHP runtime is modularly split into focused static archives:
1. `libsph_core.a`: Minimal runtime footprint (< 120 KB linked binaries). Provides multi-tier slab memory management, `Rc`/`Arc`, arenas, strings, formatting, panic handling, and math intrinsics.
2. `libsph_async.a`: Fiber context switcher, thread pool, atomic synchronization, and event loop.
3. `libsph_net.a`: Non-blocking sockets, HTTP server, RFC 6455 WebSocket engine, and `epoll` engine.
4. `libsph_compat.a`: Legacy PHP dynamic runtime, array hashing table, WinHTTP/cURL client, and SQLite3 engine.
5. `libsph_gui.a`: Cross-platform native GUI abstraction (`Win32` on Windows, `X11`/`GLX`/`epoll` on Linux) and SIMD RGBA canvas.
6. `libsph_runtime.a`: Aggregate static library containing `core + async + net + compat`.

---

## 8. Standard Library Architecture (`stdlib/`)

The 1.0 Standard Library provides normative modules located in the `$SPH_HOME/stdlib/` hierarchy:

### 8.1 Module Summary
- `std/core.sph`: Mathematical utilities (`min_i32`, `max_i32`, `abs_i32`, `clamp_i32`), assertions (`assert`), panic aborts (`panic_str`), and process exit (`process_exit`).
- `std/buffer.sph`: Binary buffer type (`ByteBuffer`), offset manipulation, slicing, integer endianness (`set_u16_le`, `set_u32_le`), and null-byte safe storage.
- `std/thread.sph`: Operating system multi-threading (`Thread::spawn`, `thread_spawn`, `thread_join`) and recursive mutexes (`Mutex`, `mutex_create`, `mutex_lock`).
- `std/io.sph`: High-performance formatted console I/O (`print_str`, `println_str`, `println_i32`, `println_f64`, `println_bool`).
- `std/fs.sph`: Cross-platform file operations (`read_file`, `write_file`, `append_file`, `file_exists`, `delete_file`).
- `std/collections.sph`: High-level data structures and iterators (`KeyValuePair`, `IntRange`, `range`, `range_length`).
- `std/async.sph`: Asynchronous fiber abstractions (`sleep_ms`, `yield_now`, `run_event_loop`).
- `std/net.sph`: Network communication primitives (`http_get`), TCP listener (`server_listen`), and zero-dependency RFC 6455 WebSocket client (`WebSocketClient`, `ws_connect`).
- `std/crypto.sph`: SHA-256 cryptographic hashing (`crypto_sha256`) and uniform random number generation (`crypto_random_u32`, `crypto_random_range`).
- `std/json.sph`: JSON string validation (`json_validate`), escaping, and object/array stringification (`json_stringify_pair`, `json_wrap_object`).
- `std/time.sph`: Monotonic microsecond clock (`instant_now`, `instant_elapsed_ms`, `instant_elapsed_micros`) and duration helpers (`Duration`).
- `std/process.sph`: Process execution, process IDs (`process_id`, `process_exit`), and environment variables (`env_get`, `env_set`).
- `std/web.sph`: Native embedded HTTP server routing, request dispatching (`Request`), response generation (`WebResponse`), and non-blocking socket multiplexing (`server_accept`, `server_poll`).
- `std/gui.sph`: Win32 native controls, event loop pumping, message boxes, and 32-bit SIMD RGBA canvas drawing.

### 8.2 Import Resolution Pipeline
When an `import "path/to/module.sph"` declaration is encountered:
1. The compiler checks for a relative path matching the current file's directory.
2. If not found, it checks `<working_directory>/stdlib/<module>`.
3. If not found, it checks `<SPH_HOME>/stdlib/<module>`.
4. If not found, a diagnostic compilation error is emitted.

---

## 9. Developer Tooling & Verification Contracts

All conformant toolchains must implement:
- `sphc build <file.sph>`: Compiles and links an executable.
- `sphc run <file.sph>`: Compiles, links, and immediately executes the binary.
- `sphc check <file.sph>`: Runs syntax and semantic analysis without code generation.
- `sphc inspect <file.sph> [--tokens|--ast|--ir|--asm|--llvm]`: Dumps intermediate stages.
- `sphc doctor`: Verifies host toolchain (GCC/Clang, ld, Win32/POSIX libraries, runtime archives, stdlib).

