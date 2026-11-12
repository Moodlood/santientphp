# SantientPHP Standard Library Reference

This reference documents the core types, built-in functions, concurrency primitives, SIMD vectors, and PHP compatibility library available in SantientPHP.

---

## 1. Primitive & Scalar Types

| Type | Size | Description |
| :--- | :--- | :--- |
| `i32` | 32-bit | Signed 32-bit two's complement integer |
| `i64` | 64-bit | Signed 64-bit two's complement integer |
| `u32` | 32-bit | Unsigned 32-bit integer |
| `u64` | 64-bit | Unsigned 64-bit integer |
| `f32` | 32-bit | IEEE 754 single-precision floating point |
| `f64` | 64-bit | IEEE 754 double-precision floating point |
| `bool` | 8-bit | Boolean value (`true` or `false`) |
| `string` | Pointer + Len | Immutable byte sequence |
| `void` | 0-bit | Unit return type |
| `dynamic` | 128-bit | Dynamic variant supporting PHP loose coercion and associative arrays |

---

## 2. 128-bit SIMD Vector Types

Hardware-accelerated vector types utilizing x86_64 SSE/AVX instructions:

- `f32x4`: Four 32-bit single-precision floats
- `i32x4`: Four 32-bit signed integers
- `f64x2`: Two 64-bit double-precision floats
- `i64x2`: Two 64-bit signed integers

### Constructors & Factories
- `new f32x4(x, y, z, w)` / `f32x4(x, y, z, w)`
- `f32x4::splat(val)`: Broadcasts a scalar across all lanes
- `f32x4::zero()`: Initializes all lanes to zero

### Operations
- **Lane Access**: `v.x`, `v.y`, `v.z`, `v.w`, or `v[i]`
- **Arithmetic**: `+`, `-`, `*`, `/`
- **Math Methods**:
  - `v.sqrt()`: Per-lane square root
  - `v.abs()`: Per-lane absolute value
  - `v.min(other)`: Per-lane minimum
  - `v.max(other)`: Per-lane maximum
  - `v.dot(other)`: Dot product reduction returning scalar `f32`
  - `v.sum()`: Horizontal sum across all lanes

---

## 3. Concurrency & Atomics

### 3.1 Hardware Atomics
- Types: `AtomicI32`, `AtomicI64`, `AtomicBool`
- Methods:
  - `load()`: Atomically loads value
  - `store(val)`: Atomically writes value
  - `fetch_add(val)`: Atomically adds and returns previous value
  - `fetch_sub(val)`: Atomically subtracts and returns previous value
  - `exchange(val)`: Atomically swaps value
  - `compare_exchange(expected, desired)`: CAS operation

### 3.2 Native OS Threading
- `Thread::spawn(fn_ptr)`: Spawns an operating system thread
- `Thread::join(thread_handle)`: Waits for thread termination
- `Thread::sleep_ms(ms)`: Suspends current thread execution

---

## 4. Coroutines & Asynchronous Runtime

- `async fn name(params): Promise<T>`: Declares cooperative coroutine
- `await <promise_expr>`: Suspends coroutine until promise resolution
- `Promise<T>`:
  - `resolve(val)`: Resolves promise with value
  - `reject(err)`: Marks promise as failed
  - `is_ready()`: Checks completion status
  - `get_value()`: Retrieves resolved value
- `EventLoop`:
  - `EventLoop::spawn(coroutine)`: Schedules a coroutine
  - `EventLoop::yield_now()`: Yields control to other ready coroutines
  - `EventLoop::run()`: Runs scheduled coroutines to completion

---

## 5. Memory Management Primitives

- `Box<T>`: Unique heap allocation with deterministic deallocation on exit
- `Rc<T>`: Reference-counted pointer for shared immutable ownership
- `Arena`: Fast linear region allocator for bulk allocations

---

## 6. PHP Compatibility Subsystem

### 6.1 Core Statements & Functions
- `echo <expr>, ...;`: PHP-compatible printing
- `print(val)`: Prints value followed by return code 1
- `var_dump(val)`: Detailed type and value debugging dump
- `count($arr)`: Returns element count of dynamic array

### 6.2 Array Functions
- `array_push(&$arr, val)`: Appends element to dynamic array
- `array_pop(&$arr)`: Removes and returns last element
- `array_keys($arr)`: Returns list of keys
- `array_values($arr)`: Returns list of values

### 6.3 String Functions
- `strlen(str)`: Length in bytes
- `strpos(haystack, needle)`: Offset of needle or -1
- `substr(str, start, length)`: Substring slice
- `explode(delim, str)`: Splits string into array
- `implode(glue, arr)`: Joins array elements into string
