# SantientPHP (SPH) 0.1 Language Specification Baseline

**Status:** Normative specification for SPH 0.1\
**Compiler:** `sphc`\
**File extension:** `.sph`

---

## 1. Introduction

SantientPHP is an independent, compiled language inspired by PHP syntax while providing native static types and predictable compiled performance.

SPH 0.1 is the initial minimal vertical slice. Its primary objective is to prove the end-to-end compilation pipeline from `.sph` source code to a native Windows x86-64 executable.

---

## 2. Source Files and Opening Tag

Every SPH 0.1 source file must begin with the PHP opening tag:

```php
<?php
```

Whitespace, newlines, and comments may precede or immediately follow `<?php`. However, executable code statements must follow the opening tag. The closing tag `?>` is omitted in pure SPH source files.

---

## 3. Lexical Structure

### 3.1 Character Encoding
SPH source code is UTF-8 encoded.

### 3.2 Whitespace
Whitespace consists of spaces (`0x20`), horizontal tabs (`0x09`), carriage returns (`0x0D`), and newlines (`0x0A`). Whitespace separates tokens and is otherwise ignored except within string literals.

### 3.3 Comments
- **Single-line comments**: Begin with `//` and extend to the end of the line.
- **Multi-line comments**: Enclosed within `/*` and `*/`. Nested multi-line comments are not supported in 0.1.

### 3.4 Identifiers
Identifiers name functions, variables, and types.
- An identifier must begin with an ASCII letter (`a-z`, `A-Z`) or underscore (`_`).
- Subsequent characters may be ASCII letters, digits (`0-9`), or underscores (`_`).

### 3.5 Keywords
The following tokens are reserved keywords in SPH 0.1+:

```text
fn      let     mut     return  if      else    while   true    false   struct  enum    import  class   this    interface implements unsafe extern as
```

### 3.6 Primitive Type Keywords
The following primitive types are recognized in SPH 0.1:

```text
i32     i64     f32     f64     bool    string  void
```

### 3.7 Literals

1. **Integer Literals**:
   - Decimal: digits `0-9` (e.g. `100`, `0`, `42`).
   - Hexadecimal: prefix `0x` or `0X` followed by hex digits `0-9`, `a-f`, `A-F` (e.g. `0xFF`).
   - Binary: prefix `0b` or `0B` followed by binary digits `0` or `1` (e.g. `0b1010`).
   - By default, untyped integer literals infer as `i32` if within range, or `i64` if exceeding 32-bit bounds.

2. **Floating-Point Literals**:
   - Decimal digits containing a dot `.` followed by one or more digits, with optional scientific exponent `e` or `E` (e.g. `3.14`, `0.5`, `1e-3`).
   - By default, untyped float literals infer as `f64`.

3. **Boolean Literals**:
   - `true` and `false`.

4. **String Literals**:
   - Double-quoted: `"..."` supporting escape sequences: `\n`, `\t`, `\r`, `\\`, `\"`, `\0`.
   - Single-quoted: `'...'` supporting escape sequences: `\'`, `\\`.

### 3.8 Operators and Punctuation

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Assignment: `=`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Logical: `&&`, `||`, `!`
- Casting: `as`
- Range: `..`
- Punctuation: `;`, `:`, `,`, `.`, `::`
- Delimiters: `(`, `)`, `{`, `}`, `[`, `]`

---

## 4. Grammar (EBNF Subset for SPH 0.1+)

```ebnf
Program         ::= "<?php" TopLevelDecl* EOF

TopLevelDecl    ::= ImportDecl | FunctionDecl | ExternFunctionDecl | StructDecl | EnumDecl | ClassDecl | InterfaceDecl

ImportDecl      ::= "import" StringLiteral ";"
ExternFunctionDecl ::= "extern" ( StringLiteral )? "fn" Identifier "(" ParameterList? ")" (":" Type)? ";"
FunctionDecl    ::= "fn" Identifier ("<" TypeParamList ">")? "(" ParameterList? ")" (":" Type)? Block
StructDecl      ::= StructAttribute* "struct" Identifier ("<" TypeParamList ">")? "{" (StructMember ("," | ";")?)* "}"
StructAttribute ::= "#[" AttributeItem "]"
AttributeItem   ::= "repr" "(" ("packed" | "C") ")" | "align" "(" IntLiteral ")"
StructMember    ::= Identifier ":" Type
EnumDecl        ::= "enum" Identifier "{" (EnumVariant ("," EnumVariant)* ","?)? "}"
EnumVariant     ::= Identifier ("=" IntLiteral)?
ClassDecl       ::= "class" Identifier ("implements" Identifier ("," Identifier)*)? "{" (ClassMember ("," | ";")?)* "}"
ClassMember     ::= ClassField | FunctionDecl
ClassField      ::= Identifier ":" Type
InterfaceDecl   ::= "interface" Identifier "{" (MethodSignature ("," | ";")?)* "}"
MethodSignature ::= "fn" Identifier "(" ParameterList? ")" (":" Type)? ";"?

TypeParamList   ::= Identifier ("," Identifier)*
TypeArgsList    ::= Type ("," Type)*
ParameterList   ::= Parameter ("," Parameter)*
Parameter       ::= Identifier ":" Type

Type            ::= "i32" | "i64" | "f32" | "f64" | "bool" | "string" | "void"
                  | "Rc" "<" Type ">"
                  | Identifier ("<" TypeArgsList ">")?
                  | "[" Type ";" IntLiteral "]"
                  | "&[" Type "]"
                  | "*" Type

Block           ::= "{" Statement* "}"

Statement       ::= VarDeclStmt
                  | ReturnStmt
                  | IfStmt
                  | WhileStmt
                  | ExprStmt
                  | UnsafeBlockStmt
                  | Block

UnsafeBlockStmt ::= "unsafe" Block
VarDeclStmt     ::= ("let" "mut"? | "mut") Identifier (":" Type)? "=" Expression ";"
ReturnStmt      ::= "return" Expression? ";"
IfStmt          ::= "if" "(" Expression ")" Block ("else" (IfStmt | Block))?
WhileStmt       ::= "while" "(" Expression ")" Block
ExprStmt        ::= Expression ";"

Expression      ::= AssignmentExpr
AssignmentExpr  ::= ((PostfixExpr | ("*" UnaryExpr)) "=")? LogicalOrExpr
LogicalOrExpr   ::= LogicalAndExpr ("||" LogicalAndExpr)*
LogicalAndExpr  ::= BitwiseOrExpr ("&&" BitwiseOrExpr)*
BitwiseOrExpr   ::= BitwiseXorExpr ("|" BitwiseXorExpr)*
BitwiseXorExpr  ::= BitwiseAndExpr ("^" BitwiseAndExpr)*
BitwiseAndExpr  ::= EqualityExpr ("&" EqualityExpr)*
EqualityExpr    ::= RelationalExpr (("==" | "!=") RelationalExpr)*
RelationalExpr  ::= ShiftExpr (("<" | "<=" | ">" | ">=") ShiftExpr)*
ShiftExpr       ::= AdditiveExpr (("<<" | ">>") AdditiveExpr)*
AdditiveExpr    ::= MultiplicativeExpr (("+" | "-") MultiplicativeExpr)*
MultiplicativeExpr ::= CastExpr (("*" | "/" | "%") CastExpr)*
CastExpr        ::= UnaryExpr ("as" Type)*
UnaryExpr       ::= ("-" | "!" | "~" | "&" | "*") UnaryExpr | PostfixExpr
PostfixExpr     ::= PrimaryExpr ("." Identifier ("(" ArgumentList? ")")? | ("<" TypeArgsList ">")? "(" ArgumentList? ")" | "[" IndexOrSlice "]")*
IndexOrSlice    ::= RangeExpr | Expression
RangeExpr       ::= Expression? ".." Expression?
PrimaryExpr     ::= Literal
                  | RcNewExpr
                  | RcCountExpr
                  | StructInitExpr
                  | ArrayLiteralExpr
                  | ScopedAccessExpr
                  | "this"
                  | Identifier
                  | "(" Expression ")"

RcNewExpr       ::= "Rc" "::" "new" "(" Expression ")"
RcCountExpr     ::= "Rc" "::" "count" "(" Expression ")"
ScopedAccessExpr::= Identifier "::" Identifier
StructInitExpr  ::= Identifier ("<" TypeArgsList ">")? "{" (Identifier ":" Expression ("," | ";")?)* "}"
ArrayLiteralExpr::= "[" (Expression ("," Expression)*)? "]"
ArgumentList    ::= Expression ("," Expression)*

Literal         ::= IntLiteral | FloatLiteral | StringLiteral | BoolLiteral
```

---

## 5. Built-in Functions in 0.1

In SPH 0.1, the compiler natively provides:
- `println(value)`: Prints the scalar value followed by a newline to standard output. Supports `i32`, `i64`, `f64`, `bool`, and `string`.

---

## 6. SPH 0.1 Canonical Acceptance Program

File `examples/hello.sph`:

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

**Expected Executable Output:**
```text
300
```

---

## 7. Systems Programming & Memory Attributes

### 7.1 Struct Representation and Layout Attributes
Struct definitions can be annotated with layout attributes:
- `#[repr(packed)]`: Disables alignment padding between struct fields. All fields are placed at contiguous offsets, yielding 1-byte alignment. Useful for binary protocol headers and wire formats.
- `#[repr(C)]`: Follows standard C ABI struct alignment and padding rules.
- `#[align(N)]`: Sets explicit alignment requirement for the struct type (where $N$ is a power of 2).

### 7.2 Heap Allocation Primitives
The runtime core provides low-level heap memory primitives:
- `sph_alloc(size: i64): *void`: Allocates `size` bytes of uninitialized heap memory.
- `sph_free(ptr: *void): void`: Releases previously allocated heap memory.
These primitives can be declared using `extern "C" fn` and called within `unsafe` blocks.

---

## 8. Safe & Managed Memory: Reference Counting (`Rc<T>`)

### 8.1 Object Header & Allocation
The `Rc<T>` type represents a strong reference-counted managed pointer to a heap-allocated value of type `T`.
- Every `Rc` allocation is prepended with a 16-byte header:
  ```c
  typedef struct {
      int64_t ref_count;
      int64_t _reserved;
  } sph_rc_header;
  ```
- `Rc::new(expr)` allocates memory on the heap via `sph_rc_alloc(sizeof(T))`, initializes the memory, and sets `ref_count = 1`.
- `Rc::count(expr)` queries the current strong reference count via `sph_rc_count`.

### 8.2 Automatic Retain & Release (ARC)
- **Copying**: Assigning an existing `Rc<T>` variable (`let q = p`) increments the reference count via `sph_rc_retain`.
- **Scope Exit & Returns**: When an `Rc<T>` binding goes out of scope or the function returns, `sph_rc_release` is automatically emitted to decrement the count.
- **Deallocation**: When `ref_count` reaches 0, the runtime frees the block immediately with zero pause or garbage collection latency.
- **Auto-Dereference**: Member fields of `T` are accessible directly via `p.field` without manual dereferencing.

### 8.3 Single-Owner Heap Pointers (`Box<T>`)
- The `Box<T>` type represents an exclusive, unique heap-allocated pointer to a value of type `T`.
- **Allocation**: `Box::new(expr)` allocates memory directly via `sph_alloc(sizeof(T))` with zero header overhead and initializes the heap-allocated value.
- **Size**: 8 bytes (pointer size).
- **Auto-Dereference**: Field access on boxes (`b.field`) automatically dereferences the underlying pointer.
- **Deterministic Destruction**: When an active (non-moved) `Box<T>` variable goes out of scope or is overwritten by assignment, it is immediately freed via `sph_free(ptr)`.

### 8.4 Move Semantics & Compile-Time Move Checking
- **Move-Only Classification**: `Box<T>`, `Arena`, and composite types containing them are classified as move-only (`isMoveOnly() == true`).
- **Ownership Transfer**: Passing a move-only type by value into another variable (`let b2 = b1;`), into a function call (`consume(b2);`), or returning it transfers single ownership from the source binding to the destination.
- **Compile-Time Diagnostics**: Accessing a moved variable produces an immediate compiler diagnostic:
  `error: use of moved value '<name>'`
- **Reinitialization**: Assigning a new value into a mutable moved variable re-establishes valid ownership and clears the moved state.

### 8.5 Arena Allocation & Lifetime Scopes (`Arena`)
- **Arena Type**: The `Arena` type represents a bump-pointer memory allocator that amortizes allocation overhead into large contiguous memory chunks (default 64 KB or custom capacity).
- **Creation**:
  - `Arena::new()` allocates an arena handle with default chunk size (64 KB).
  - `Arena::new(capacity)` allocates an arena handle with an initial chunk capacity.
- **Bump Allocation**:
  - `Arena::alloc(arena, value)` or `arena.alloc(value)` allocates memory aligned to `T`'s requirements in $O(1)$ and initializes the value.
  - Returns a raw typed pointer `*T` referencing the arena-allocated instance.
- **Bulk Reset ($O(1)$)**:
  - `Arena::reset(arena)` or `arena.reset()` rewinds the chunk bump offset back to 0 without deallocating chunks back to the OS, enabling rapid phase-based memory reuse.
- **Destruction**:
  - `Arena::destroy(arena)` or `arena.destroy()` frees all allocated chunks and the arena handle itself.
  - Moving or destroying an arena invalidates subsequent accesses to the handle via compile-time move checking.

---

## 9. Runtime & Standard Library: Strings and Collections

### 9.1 Native Strings
- **Type**: `string` is a pointer to a null-terminated UTF-8 byte buffer.
- **Concatenation (`+`)**: Adding two strings (`s1 + s2`) allocates a new null-terminated heap buffer via `sph_string_concat` containing both strings.
- **Equality (`==`, `!=`)**: Comparing two strings tests content equality via `sph_string_eq` rather than pointer identity.
- **Length (`.len()`)**: Querying `s.len()` returns the byte length of the string as `i64`.

### 9.2 Dynamic Vectors (`Vec<T>`)
- **Descriptor**: 8-byte move-only handle pointing to a heap descriptor with `data` pointer, active element count `len`, buffer capacity `cap`, and element size `elem_size`.
- **Construction**:
  - `Vec::new()` / `Vec<T>::new()`: Creates a dynamic vector with default initial capacity (4).
  - `Vec::with_capacity(cap)`: Pre-allocates backing storage for `cap` elements.
- **Operations**:
  - `v.push(item)`: Appends an item to the end, doubling capacity via `realloc` when full.
  - `v.pop()`: Removes and returns the last element.
  - `v.len()`: Queries current element count (`i64`).
  - `v.capacity()`: Queries allocated capacity (`i64`).
  - `v[index]`: Direct element indexing for reading and writing with runtime bounds checking.
  - `v.destroy()`: Releases the buffer and vector descriptor. Subsequent accesses are rejected at compile time via move checking.

### 9.3 Standard Filesystem, Process & Environment APIs

The SantientPHP standard library provides native, zero-overhead static APIs for interacting with the operating system, filesystem, and runtime environment:

- **Filesystem (`File`)**:
  - `File::read(path: string): string` — Reads the complete file into a newly allocated heap string. Returns an empty string `""` on I/O error or non-existent file.
  - `File::write(path: string, content: string): bool` — Writes string content to disk (overwriting existing file). Returns `true` on success, `false` on error.
  - `File::append(path: string, content: string): bool` — Appends string content to the end of the file. Returns `true` on success, `false` on error.
  - `File::exists(path: string): bool` — Tests whether a file exists at the given path.
  - `File::delete(path: string): bool` — Deletes the file at the given path. Returns `true` on success, `false` on error.

- **Process (`Process`)**:
  - `Process::exit(code: i32): void` — Immediately terminates execution of the current process with exit code `code`.

- **Environment (`Env`)**:
  - `Env::get(name: string): string` — Retrieves the value of environment variable `name` as a new heap-allocated string. Returns `""` if not set.
  - `Env::set(name: string, value: string): bool` — Sets environment variable `name` to `value`. Returns `true` on success, `false` on error.

### 9.4 Error Handling Primitives: `Result<T, E>` and `panic`

SantientPHP employs value-based error handling without C++ exception overhead:

- **Type**: `Result<T, E>` represents either success containing value `T` (`Ok`) or failure containing error `E` (`Err`).
- **Constructors**:
  - `Ok(val: T)` or `Result::ok(val)` creates a successful result.
  - `Err(err: E)` or `Result::err(err)` creates an error result.
- **Operations & Methods**:
  - `res.is_ok(): bool` — Returns `true` if `res` is `Ok`, `false` otherwise.
  - `res.is_err(): bool` — Returns `true` if `res` is `Err`, `false` otherwise.
  - `res.unwrap(): T` — Extracts and returns the underlying `T` value. If called on an `Err`, aborts process execution with a descriptive panic message.
  - `res.destroy(): void` — Releases the result container.
- **Runtime Panic**:
  - `panic(msg: string): void` — Prints `panic: <msg>` to `stderr` and aborts process execution via `abort()`.

---

## 10. PHP Compatibility Subsystem

The PHP compatibility subsystem provides optional dynamic execution semantics and classic PHP syntax interoperability without compromising native static performance.

### 10.1 Project Manifest Configuration (`sph.toml`)

Projects specify configuration and compatibility modes in `sph.toml`:

```toml
[project]
name = "my_project"
version = "0.1.0"
entry = "src/main.sph"

[compiler]
language_version = "0.1"

[target]
triple = "x86_64-windows"

[build]
mode = "debug"
optimization = "0"

[compatibility]
php = true
mode = "dynamic" # "strict" | "transitional" | "dynamic"
```

- In `strict` mode, variables require explicit `let` or `mut` bindings, and static type rules are enforced.
- In `dynamic` mode, variables with leading `$` are implicitly declared upon assignment and support arbitrary runtime type reassignments.

### 10.2 Dynamic Value Representation (`dynamic` / `sph_value`)

- **Descriptor**: 16-byte tagged union (`type: i32`, `_reserved: i32`, `data: 64-bit payload`).
- **Types**: `NULL`, `BOOL`, `INT`, `FLOAT`, `STRING`, `ARRAY`, `OBJECT`.
- **Coercion Semantics**:
  - Boolean truthiness matches PHP: `"0"`, `""`, `0`, `0.0`, and empty arrays evaluate to `false`. All other values evaluate to `true`.
  - Numeric strings participating in arithmetic operations are automatically coerced to integer or float values (e.g. `10 + "50"` evaluates to `60`).
  - Booleans stringify as `"1"` (true) or `""` (false), conforming to PHP `echo` output.

### 10.3 PHP Variables, Echo & String Concatenation

- **Variable Syntax**: Variables beginning with `$` (e.g. `$user`, `$count`) represent dynamic bindings.
- **String Concatenation (`.`)**: The dot operator (`.`) performs string concatenation on string and dynamic operands (`"Hello " . $user`).
- **Echo Statement (`echo`)**: The `echo expr1, expr2;` statement evaluates and prints expressions without automatically appending a trailing newline.

### 10.4 PHP-Style Associative Arrays (`sph_array`)

- **Structure**: Ordered hash table maintaining insertion sequence via an internal doubly-linked list.
- **Keys**: Supports mixed integer (`int64_t`) and string (`char*`) keys.
- **Key Normalization**: Decimal integer string keys without leading zeros (e.g. `"42"`, `"-10"`) are automatically converted to integers.
- **Syntax**:
  - Array literals: `["key" => $val, 0 => "first", "trailing",]`
  - Index read: `$arr[$key]`
  - Index write: `$arr[$key] = $val`
  - Array append: `$arr[] = $val` (automatically assigns and increments `next_int_key`)
- **Auto-Vivification**: Assigning into an uninitialized or null variable (`$nullVar["key"] = 10;` or `$empty[] = 20;`) automatically allocates a dynamic array.
- **Debug Inspection**: Built-in `var_dump($arr)` formats and outputs array keys, types, and values in insertion order.

### 10.5 PHP Loops & Core Standard Library

- **Foreach Loops (`foreach`)**:
  - Value iteration: `foreach ($arr as $val) { ... }`
  - Key-value iteration: `foreach ($arr as $key => $val) { ... }`
  - Supports both dynamic arrays (`sph_array`) and fixed native arrays (`[T; N]`).
- **Loop Control**: `break;` and `continue;` statements control loop execution with nesting validation.
- **Array Utilities**:
  - `count($arr)`: Returns total element count.
  - `array_push($arr, $val)`: Pushes a new element and returns new length.
  - `array_pop($arr)`: Pops the last inserted element.
  - `array_shift($arr)`: Shifts and returns the first inserted element.
  - `array_keys($arr)` / `array_values($arr)`: Extracts keys or values as an indexed array.
  - `in_array($needle, $haystack)`: Checks presence of a value with PHP loose coercion equality.
- **String Utilities**:
  - `strlen($str)`: Returns byte length of string.
  - `substr($str, $start, [$len])`: Extracts a substring.
  - `strpos($haystack, $needle)`: Returns 0-based offset or -1 on failure.
  - `explode($delimiter, $str)`: Splits string into an indexed array.
  - `implode($glue, $arr)`: Joins array elements into a string.

### 10.6 PHP Dynamic & Static Objects, `new`, `->` Operator, and `$this`

- **Object Creation (`new`)**:
  - Syntax: `new ClassName(arg1, arg2)` instantiates a class, invokes `__construct` (or class-name constructor), and returns the instance.
- **Property & Method Access (`->`)**:
  - Property access: `$obj->prop` reads or `$obj->prop = $val` writes to an object property.
  - Method invocation: `$obj->method(arg1, arg2)` calls a member function on the receiver.
- **Receiver Binding (`$this` / `this`)**:
  - Inside class methods, `$this` is automatically bound to the current instance receiver and permits property access and method calls.
- **Visibility Modifiers**:
  - `public`, `private`, `protected` visibility specifiers are recognized for fields and methods.
- **Object Utilities**:
  - `is_object($val)`: Returns `true` (1) if `$val` is an object instance, `false` (0) otherwise.
  - `get_class($val)`: Returns the string class name of the object.
  - `property_exists($obj, $prop)`: Returns `true` (1) if the specified property exists on the object, `false` (0) otherwise.
- **Dynamic Object Representation (`sph_object`)**:
  - Objects in dynamic context are wrapped in `sph_value` with type `SPH_TYPE_OBJECT` (6), containing a class name, native instance pointer, and dynamic properties map.
  - `var_dump($obj)` formats object contents according to standard PHP formatting: `object(ClassName)#1 (count) { ["prop"]=> type(val) ... }`.

### 10.7 PHP Exception Handling (`try`, `catch`, `finally`, `throw`)

- **Syntax**:
  ```php
  try {
      if ($err) {
          throw new Exception("Something failed");
      }
  } catch (Exception $e) {
      echo "Caught: " . $e->message . "\n";
  } finally {
      echo "Always executed\n";
  }
  ```
- **Control Flow**:
  - `throw expr;`: Evaluates the thrown expression, wraps structured class instances into heap-allocated dynamic exception objects (`sph_object`), and unwinds the runtime call stack to the nearest active `try` block.
  - `try { ... }`: Enters a protected scope by pushing an `sph_exception_frame` onto the thread-local runtime frame stack.
  - `catch (Type $var) { ... }`: Dispatches caught exceptions matching the specified class type (e.g. `Exception`) or any exception if untyped. Binds the exception instance to `$var` within the catch body scope.
  - `finally { ... }`: Guaranteed execution block running after either normal try completion or catch clause execution before transferring control to downstream code.
- **Unwinding Architecture**:
  - Implemented via low-overhead non-SEH register/stack restore primitives (`sph_setjmp` and `sph_longjmp`) on Windows x86-64, preserving callee-saved registers (`rbx`, `rbp`, `rsp`, `r12`-`r15`, and `xmm6`-`xmm15`) without relying on PE unwind tables.
  - Fatal unhandled exceptions terminate the process with standard PHP message formatting: `Fatal error: Uncaught Exception: <message>`.

### 10.8 PHP Pass-By-Reference Parameter Semantics (`&$var`)

- **Syntax**:
  - Untyped dynamic reference: `function foo(&$val) { $val = $val + 1; }`
  - Typed reference (PHP 7+ / SPH): `function bar(int &$val) { ... }` or `function baz(i32 &$val) { ... }`
  - Explicit SPH syntax: `fn qux(&val: i32) { ... }`
- **Semantics**:
  - Reference parameters are passed implicitly as pointers to the caller's storage slot (`addressof %var`).
  - Inside the callee, reads from the parameter automatically dereference the pointer, and assignments write directly into the caller's stack slot in-place.
  - Forwarding a by-reference parameter from caller to callee preserves the pointer without double-indirection.

### 10.9 Database Interoperability (`PDO` & SQLite)

- **Zero-Dependency Architecture**:
  - Dynamically loads Microsoft Windows built-in `C:\Windows\System32\winsqlite3.dll` (with fallback to `sqlite3.dll`) at runtime via `LoadLibraryA`.
  - Zero external third-party binary runtime dependencies required.
- **`PDO` Class**:
  - Constructor: `new PDO(string $dsn [, ?string $username = null, ?string $password = null])`.
  - DSN: Supports `sqlite::memory:` for in-memory databases or `sqlite:<filename>` for disk-based databases.
  - `$pdo->exec(string $sql): int`: Executes raw DDL or non-query SQL statements and returns the number of modified rows.
  - `$pdo->prepare(string $sql): PDOStatement`: Prepares a parameterized SQL statement supporting `?` positional placeholders and `:name` named placeholders.
  - `$pdo->query(string $sql): PDOStatement`: Prepares and immediately executes a query.
  - `$pdo->lastInsertId(): int`: Retrieves the rowid of the most recently inserted row.
- **`PDOStatement` Class**:
  - `$stmt->execute(?array $params = null): bool`: Binds dynamic parameter arrays (positional `?` from 0-indexed arrays and named `:key` or `key` from associative arrays) and steps non-query statements or prepares queries for row fetching.
  - `$stmt->fetch(int $mode = PDO::FETCH_ASSOC): dynamic`: Fetches the next row as an associative or numeric array, or returns `false` when no rows remain.
  - `$stmt->fetchAll(int $mode = PDO::FETCH_ASSOC): array`: Fetches all remaining result rows into a sequential array of row arrays.
  - `$stmt->rowCount(): int`: Returns the count of affected rows.
  - `$stmt->columnCount(): int`: Returns the number of columns in the result set.
  - `$stmt->close(): void`: Finalizes the prepared statement.
- **Constants**:
  - `PDO::FETCH_ASSOC` (2), `PDO::FETCH_NUM` (3), `PDO::FETCH_BOTH` (4).

### 10.10 HTTP & Web Interoperability (`curl_*` & `file_get_contents`)

- **Zero-Dependency Architecture**:
  - Dynamically loads Microsoft Windows built-in `winhttp.dll` (`WinHttpOpen`, `WinHttpConnect`, `WinHttpOpenRequest`, `WinHttpSendRequest`, `WinHttpReceiveResponse`, `WinHttpReadData`, `WinHttpCloseHandle`).
  - For offline environments and deterministic unit testing, requests targeting `http://mock/...` or `mock://...` are intercepted and handled with simulated REST responses.
- **cURL Procedural API**:
  - `curl_init(?string $url = null): dynamic`: Initializes a cURL handle.
  - `curl_setopt(dynamic $ch, int $option, dynamic $value): bool`: Sets options:
    - `CURLOPT_URL` (10002): Target request URL.
    - `CURLOPT_RETURNTRANSFER` (19913): If true, returns response body as string; if false, echoes body and returns boolean.
    - `CURLOPT_POST` (47): Enables HTTP POST method.
    - `CURLOPT_POSTFIELDS` (10015): Sets POST body content.
    - `CURLOPT_HTTPHEADER` (10023): Sets request headers from an array of strings.
    - `CURLOPT_TIMEOUT` (13): Sets request timeout in seconds.
    - `CURLOPT_CUSTOMREQUEST` (10036): Sets custom HTTP method (PUT, DELETE, PATCH, etc.).
  - `curl_exec(dynamic $ch): dynamic`: Executes the request and returns the response body or boolean status.
  - `curl_getinfo(dynamic $ch, int $opt = CURLINFO_HTTP_CODE): dynamic`: Retrieves status information (e.g. HTTP status code `200`, `404`, etc.).
  - `curl_close(dynamic $ch): void`: Closes the handle and releases resources.
- **File Retrieval with Remote URLs (`file_get_contents`)**:
  - `file_get_contents(string $path_or_url): string`:
    - If the target begins with `http://`, `https://`, or `mock://`, delegates directly to the built-in HTTP client service (`sph_http_get`) and returns the response payload.
    - If targeting local files, reads binary/text file contents from disk.

### 10.11 PHP Compatibility Boundaries, Supported Subset, and Deliberate Non-Goals

The SantientPHP 0.1 release delivers a clearly defined, thoroughly verified, high-performance subset of PHP suitable for standalone native compilation.

#### Supported PHP Feature Subset
1. **Dynamic Loose Typing & Operators**:
   - PHP variables prefixed with `$` (e.g. `$name`, `$price`).
   - Loose type coercion matching PHP truthiness rules (`"0"`, `""`, `0`, `0.0`, empty arrays evaluate to `false`).
   - Arithmetic expressions (`+`, `-`, `*`, `/`) with automatic string-to-number parsing and type widening.
   - Loose comparison operators (`==`, `!=`, `<`, `<=`, `>`, `>=`).
   - String concatenation operator (`.`) and `echo` statement.
2. **Associative Arrays & Standard Operations**:
   - Dual-mode arrays (`sph_array`) maintaining insertion order via hash map and doubly-linked list.
   - Mixed integer and string keys with key normalization (`"42"` normalizes to integer `42`).
   - Auto-vivification on assignment and empty bracket append (`$arr[] = $elem`).
   - Core builtins: `count`, `array_push`, `array_pop`, `array_shift`, `array_keys`, `array_values`, `in_array`, `array_key_exists`.
   - String helpers: `strlen`, `substr`, `strpos`, `explode`, `implode`.
3. **Control Flow & Loops**:
   - Value iteration: `foreach ($collection as $item) { ... }`.
   - Key-value iteration: `foreach ($collection as $key => $val) { ... }`.
   - Loop jump statements: `break`, `continue`.
4. **Object-Oriented Programming (OOP)**:
   - PHP class declarations with visibility keywords (`public`, `protected`, `private`).
   - Constructor dispatch via `__construct` (and PHP 4 fallback `ClassName`).
   - Object instantiation (`new ClassName(...)`), property access (`$obj->prop`), and method invocation (`$obj->method(...)`).
   - `$this` dynamic pointer binding.
   - Object reflection helpers: `is_object`, `get_class`, `property_exists`.
5. **Exceptions & Reference Semantics**:
   - `try`, `catch`, `finally`, `throw` with exception hierarchy and stack unwinding.
   - Pass-by-reference parameter passing (`function modify(&$ref, $val)`).
6. **Zero-Dependency Database & HTTP Interoperability**:
   - `PDO` SQLite database engine backed by native Windows `winsqlite3.dll` (`exec`, `query`, `prepare`, `execute`, `fetch`, `fetchAll(PDO::FETCH_ASSOC)`, `rowCount`, `columnCount`, `lastInsertId`, `close`).
   - cURL procedural interface backed by native Windows `winhttp.dll` (`curl_init`, `curl_setopt`, `curl_exec`, `curl_getinfo`, `curl_close`).
   - Remote URL fetching with `file_get_contents("http://...")`.

#### Explicit Semantic Differences
- **Ahead-of-Time Native Compilation**: Code compiles directly to optimized native Windows x86-64 machine code via LLVM / assembly; there is no intermediate VM or interpreter runtime.
- **Syntactic Robustness**: Block statements (`if`, `while`, `foreach`) mandate curly braces `{ ... }` to prevent dangling else and single-line control vulnerabilities.
- **Zero Third-Party Binary Dependencies**: Database and networking leverage native operating system libraries only (`winsqlite3.dll`, `winhttp.dll`).

#### Deliberate Non-Goals for 0.1
- **Dynamic Code Evaluation (`eval()`)**: Evaluated runtime strings cannot be compiled ahead-of-time safely; prohibited by design.
- **Variable-Variables (`$$foo`)**: Dynamic symbol lookup at arbitrary scope breaks static register allocation and escape analysis.
- **Global Mutable Superglobals (`$_SERVER`, `$_POST`, `$_GET`, `$_SESSION`)**: Ambient mutable globals are deprecated in favor of structured request contexts and `Env::get()`.
- **Legacy PHP 4/5 Artifacts**: Deprecated extensions (`mysql_*`, `ereg_*`, `register_globals`, `magic_quotes`) are not supported.
- **Zend VM Internal C API**: SantientPHP implements its own lightweight native C runtime (`sph_runtime`), independent of Zend engine internals.

---

## 11. Advanced Native Features & Concurrency

SantientPHP provides first-class primitives for systems-level concurrency, multi-threading, and hardware-accelerated memory models. Concurrency in SantientPHP is built around native OS threads and lock-free atomic types with zero third-party binary runtime dependencies.

### 11.1 Atomic Types & Primitives
SantientPHP defines native atomic wrappers around primitive scalar types:
- `AtomicI32` (or generic `Atomic<i32>`): 32-bit signed lock-free atomic integer.
- `AtomicI64` (or generic `Atomic<i64>`): 64-bit signed lock-free atomic integer.
- `AtomicBool` (or generic `Atomic<bool>`): 32-bit aligned lock-free atomic boolean.

Atomics can be instantiated on the stack or heap:
```php
let counter = new AtomicI32(0);
let flag = new AtomicBool(false);
let big_counter = new Atomic<i64>(1000000000);
```

### 11.2 Memory Ordering Models (`MemoryOrder`)
Atomic operations support standard ISO C++20 / LLVM memory ordering semantics:
- `MemoryOrder::Relaxed` (0): Guarantees atomicity of the single operation; imposes no cross-thread memory synchronization or instruction reordering constraints.
- `MemoryOrder::Acquire` (1): Acquire barrier; prevents subsequent reads and writes in the current thread from being reordered before this operation.
- `MemoryOrder::Release` (2): Release barrier; prevents preceding reads and writes in the current thread from being reordered after this operation.
- `MemoryOrder::AcqRel` (3): Combines Acquire and Release semantics on read-modify-write operations.
- `MemoryOrder::SeqCst` (4): Full sequential consistency; enforces a single globally agreed execution order across all sequential-consistency operations (default).

### 11.3 Atomic Operations
All atomic types expose non-blocking hardware intrinsics:
- `load([order: MemoryOrder = SeqCst]): T`: Atomically loads the current value.
- `store(val: T, [order: MemoryOrder = SeqCst]): void`: Atomically writes a new value.
- `fetch_add(val: T, [order: MemoryOrder = SeqCst]): T`: Atomically adds `val` and returns the previous value (available on `AtomicI32` and `AtomicI64`).
- `fetch_sub(val: T, [order: MemoryOrder = SeqCst]): T`: Atomically subtracts `val` and returns the previous value (available on `AtomicI32` and `AtomicI64`).
- `exchange(val: T, [order: MemoryOrder = SeqCst]): T`: Atomically swaps the value with `val` and returns the previous value.
- `compare_exchange(expected: T, desired: T, [succ: MemoryOrder = SeqCst], [fail: MemoryOrder = succ]): bool`: Atomically compares the stored value against `expected`; if equal, updates to `desired` and returns `true`. If not equal, leaves the stored value unchanged and returns `false`.

### 11.4 Native Threading Subsystem (`Thread`)
SantientPHP interfaces directly with native operating system kernel threading APIs (Windows `kernel32.dll` via `CreateThread`, `WaitForSingleObject`, `CloseHandle`, `Sleep`):
- `Thread::spawn(func, arg): i64`: Spawns a new native OS thread executing `func(arg)`. Returns a 64-bit thread handle.
- `Thread::join(handle: i64): void`: Blocks the calling thread until the target thread terminates, then safely cleans up OS thread resources.
- `Thread::sleep_ms(ms: i32): void`: Suspends the execution of the calling thread for the specified duration in milliseconds.

### 11.5 Async / Await & Cooperative Coroutine Runtime
SantientPHP provides first-class asynchronous programming via stackful cooperative coroutines powered natively by Windows Fibers (`ConvertThreadToFiber`, `CreateFiber`, `SwitchToFiber`, `DeleteFiber` in `kernel32.dll`) with zero external runtime dependencies.

#### 11.5.1 The `Promise<T>` Type
A `Promise<T>` represents the eventual result of an asynchronous computation. It exposes three primary lifecycle states: `Pending`, `Fulfilled`, and `Rejected`.
- `new Promise<T>()`: Creates a new pending promise.
- `promise.resolve(value: T): void`: Resolves the promise to `Fulfilled` and stores `value`. Wakes any coroutine fiber waiting on this promise.
- `promise.reject(error: any): void`: Resolves the promise to `Rejected` and stores `error`. Wakes waiting fibers.
- `promise.is_ready(): bool`: Returns `true` if the promise is resolved (fulfilled or rejected), `false` if still pending.
- `promise.get_value(): T`: Returns the resolved value or error payload.
- `promise.await(): T`: Waits for the promise to resolve and yields execution if called within a fiber.

#### 11.5.2 Async Functions
Functions declared with the `async` modifier (`async fn` or `async function`) are automatically executed as stackful coroutines:
```php
async fn compute(id: i32): Promise<i32> {
    EventLoop::yield_now();
    return id * 100;
}
```
Invoking an async function spawns a lightweight fiber onto the event loop ready queue and immediately returns an associated `Promise<T>`. When the function executes to completion, its return value automatically resolves the promise.

#### 11.5.3 The `await` Expression
The `await` unary operator pauses execution until the target `Promise<T>` is resolved:
```php
let val: i32 = await compute(5);
```
- When evaluated inside a coroutine fiber, `await` cooperatively yields control back to the event loop until the promise is fulfilled, then seamlessly resumes execution without blocking the OS thread.
- When evaluated on the main thread, `await` automatically pumps the event loop until the promise resolves.

#### 11.5.4 Cooperative EventLoop Scheduler
The `EventLoop` static class orchestrates cooperative task execution:
- `EventLoop::yield_now(): void`: Voluntarily suspends the currently executing fiber, places it at the tail of the ready queue, and switches to the next queued task.
- `EventLoop::spawn(func, arg): Promise<T>`: Enqueues a standalone task to execute as a coroutine fiber and returns its `Promise`.
- `EventLoop::run(): void`: Drives the event loop, continuously switching among ready fibers until all queued cooperative tasks have completed and their fiber stacks are reclaimed.

### 11.6 SIMD (Single Instruction, Multiple Data) & Target CPU Feature Detection
SantientPHP provides first-class native SIMD vector types and hardware CPU feature detection primitives, enabling high-performance parallel data processing with direct 128-bit hardware acceleration (SSE, SSE2, SSE3, SSE4.1, AVX) on x86_64 platforms alongside portable fallback implementations on all architectures.

#### 11.6.1 SIMD Vector Types
SantientPHP includes four 128-bit fixed-width SIMD vector types:
- `f32x4`: 4x 32-bit single-precision floating-point vector (16 bytes, 16-byte aligned).
- `i32x4`: 4x 32-bit signed integer vector (16 bytes, 16-byte aligned).
- `f64x2`: 2x 64-bit double-precision floating-point vector (16 bytes, 16-byte aligned).
- `i64x2`: 2x 64-bit signed integer vector (16 bytes, 16-byte aligned).

#### 11.6.2 CPU Feature Detection (`Simd`)
The `Simd` class exposes static queries that inspect hardware capabilities via CPUID:
- `Simd::has_sse(): bool`: Returns `true` if 128-bit SSE instruction set is supported.
- `Simd::has_sse2(): bool`: Returns `true` if SSE2 instructions are supported.
- `Simd::has_sse3(): bool`: Returns `true` if SSE3 instructions are supported.
- `Simd::has_sse41(): bool`: Returns `true` if SSE4.1 instructions are supported.
- `Simd::has_avx(): bool`: Returns `true` if 256-bit AVX instruction set is supported.
- `Simd::has_avx2(): bool`: Returns `true` if AVX2 instruction set is supported.
- `Simd::has_neon(): bool`: Returns `true` if ARM NEON vector instructions are supported.

#### 11.6.3 Vector Construction & Initialization
Vectors can be initialized via element-wise constructors or static factory methods:
```php
let v1 = new f32x4(1.0, 2.0, 3.0, 4.0); // Element constructor
let v2 = f32x4(5.0, 6.0, 7.0, 8.0);     // Value-style constructor
let v_zero = f32x4::zero();             // All lanes set to 0.0
let v_splat = f32x4::splat(10.0);       // All lanes broadcast to 10.0
```

#### 11.6.4 Lane Access, Indexing & Mutation
Individual vector lanes can be accessed through named fields, index expressions, or method calls:
- Named components: `v.x` (lane 0), `v.y` (lane 1), `v.z` (lane 2 for 4-lane vectors), `v.w` (lane 3 for 4-lane vectors).
- Subscript indexing: `v[i]` reads lane `i`. `v[i] = val` updates lane `i`.
- Method access: `v.get(i)` retrieves lane `i`. `v.set(i, val)` mutates lane `i`.

#### 11.6.5 Vector Arithmetic & Mathematics
SIMD vectors support standard operators and chained mathematical transformations:
- **Arithmetic operators**: `+`, `-`, `*`, `/` perform lane-wise parallel computation:
  ```php
  let v_add = v1 + v2; // [v1[0]+v2[0], v1[1]+v2[1], v1[2]+v2[2], v1[3]+v2[3]]
  let v_sub = v2 - v1;
  let v_mul = v1 * v2;
  let v_div = v2 / v1;
  ```
- **Math methods**:
  - `v.sqrt()`: Computes parallel square root across all lanes.
  - `v.abs()`: Computes parallel absolute value across all lanes.
  - `v.min(other)`: Selects minimum lane values in parallel.
  - `v.max(other)`: Selects maximum lane values in parallel.

#### 11.6.6 Reductions & Dot Product
Horizontal aggregations condense vectors into scalar quantities:
- `v.sum()`: Calculates the horizontal sum of all vector lanes (`v[0] + v[1] + ...`).
- `v.dot(other)`: Calculates the scalar dot product (`v[0]*other[0] + v[1]*other[1] + ...`).

#### 11.6.7 Memory Operations & String Formatting
- `T::load(ptr)`: Reads 16 contiguous bytes from memory into a vector.
- `T::store(ptr, v)`: Writes 16 contiguous bytes from vector `v` to memory.
- `v.to_string()` / `println(v)`: Formats vectors into human-readable strings (e.g., `f32x4(1, 2, 3, 4)`).

### 11.7 Compiler Optimizer & Optimization Pipelines

#### 11.7.1 Optimization Levels
The SPH compiler (`sphc`) and manifest (`sph.toml`) support five standard optimization tiers:
- **`-O0`**: No optimizations. Emits debug-friendly unoptimized IR with standard stack frame layout and variable slots.
- **`-O1`**: Basic optimizations. Runs memory-to-register store-to-load forwarding, constant folding, algebraic simplification, and dead code elimination.
- **`-O2`**: Standard release pipeline (default production build). Runs store-to-load forwarding, constant folding, algebraic simplification, common subexpression elimination (CSE), control flow simplification, and dead code elimination iterating to fixed point (up to 8 passes).
- **`-O3`**: Aggressive optimizations. Includes all `-O2` optimizations plus interprocedural leaf function inlining (`FunctionInliningPass`) and subsequent cascaded scalar simplifications.
- **`-Os`**: Code-size optimization. Configures the `-O2` pipeline alongside linker stripping (`-s`) to minimize binary footprint.

#### 11.7.2 Pass Infrastructure & PassManager Pipeline
The optimization pipeline operates directly on the SPH Intermediate Representation (SPHIR):
- `Pass`: Abstract base class providing lifecycle and name introspection (`getName()`).
- `FunctionPass`: Operates within the scope of an individual `IRFunction`.
- `ModulePass`: Operates across the whole `IRModule` (e.g., function inlining across call graphs).
- `PassManager`: Pipeline coordinator that sequences module passes followed by fixed-point function passes with IR re-verification.

#### 11.7.3 Integrated Optimizer Passes
- **`StoreToLoadForwardingPass`**: Analyzes local stack `AllocaInst` memory operations. Performs intra-block store-to-load forwarding and entry-block single-definition promotion, eliminating redundant stack loads.
- **`ConstantFoldingPass`**: Evaluates arithmetic, bitwise, logical, comparison, and unary expressions with compile-time known constant operands. Folds constant conditional branches (`CondBranchInst`) into unconditional jumps (`BranchInst`).
- **`AlgebraicSimplificationPass`**: Simplifies algebraic identities (`x + 0 -> x`, `x * 1 -> x`, `x - 0 -> x`, `x & 0 -> 0`, `x | 0 -> x`), self-operations (`x - x -> 0`, `x / x -> 1`, `x & x -> x`, `x | x -> x`, `x ^ x -> 0`), and annihilators (`x * 0 -> 0`). Performs strength reduction for powers of two (`x * 2^k -> x << k`, `x / 2^k -> x >> k`).
- **`CommonSubexpressionEliminationPass`**: Detects syntactically identical side-effect-free expressions within basic blocks. Accounts for operator commutativity (`a + b == b + a`, `a * b == b * a`) to eliminate redundant instructions.
- **`ControlFlowOptimizationPass`**: Collapses conditional branches with identical targets into unconditional branches and bypasses empty trampoline basic blocks.
- **`DeadCodeEliminationPass`**: Eliminates unreachable basic blocks via CFG reachability analysis, removes instructions trailing basic block terminators, and cascades deletion of unused side-effect-free instructions.
- **`FunctionInliningPass`**: Identifies small leaf functions (no internal calls, instruction count below threshold) and inlines them directly at caller call sites, remapping parameters and return values.

### 11.8 Debugger Integration & DWARF / CFI Information

#### 11.8.1 Debug Information Flags & Manifest Configuration
The SPH compiler (`sphc`) provides comprehensive native debug symbol generation compatible with GDB and LLDB:
- **CLI Flags**:
  - `-g` / `-g2`: Standard debug information level. Emits full DWARF line tables (`.file`, `.loc`), Call Frame Information (CFI), and preserves all symbolic debug sections during linking (omits `-s`).
  - `-g1`: Minimal line tables for backtrace resolution.
  - `-g3`: Extended debug information with inline locations and macro descriptions.
  - `-g0`: Disables debug symbol generation (equivalent to release stripped mode).
- **Manifest Configuration**:
  ```toml
  [build]
  debug = true
  ```
  Enabling `debug = true` in `sph.toml` automatically configures debug builds during `sphc build` and `sphc run`.

#### 11.8.2 Source Location Tracking Across Compiler Pipeline
The compiler maintains strict source location correspondence from lexical analysis down to assembly emission:
1. **SourceManager**: Maps global character offsets to 1-based line numbers and 1-based column offsets, maintaining a registry of indexed source files (`fileId`).
2. **AST**: All expression and statement nodes retain their original `SourceSpan` / `SourceLocation`.
3. **SPHIR**: Every `IRInstruction` and `IRFunction` carries a `SourceLocation` (`fileId`, `line`, `col`). `IRGenerator` attaches location tracking across all statements and expressions using RAII `LocGuard` scopes.
4. **IRModule Source Registry**: Retains the complete list of source file paths indexed by `fileId` for code generator emission.

#### 11.8.3 DWARF Line Tables & Assembly Directives
When debug information is enabled (`-g`), `X86_64CodeGen` emits standard DWARF line directives consumed by GNU `as` and LLVM `mc`:
- `.file <fileId> "<filePath>"`: Emitted at the top of the assembly file for each source file in the compilation unit.
- `.loc <fileId> <line> <col>`: Emitted before every machine instruction and function prologue, mapping assembly instructions to the exact line and column in the `.sph` source file.

#### 11.8.4 Call Frame Information (CFI) Directives
To enable accurate call stack backtraces in GDB and LLDB across function boundaries and exception frames, the code generator emits GNU CFI directives:
- `.cfi_startproc`: Marks the function entry point.
- `.cfi_def_cfa_offset 16`: Records stack pointer adjustment following `push rbp`.
- `.cfi_offset rbp, -16`: Records the saved frame pointer offset relative to CFA.
- `.cfi_def_cfa_register rbp`: Switches the Canonical Frame Address (CFA) base register to `rbp`.
- `.cfi_endproc`: Marks the termination of the function call frame.

#### 11.8.5 LLVM IR Debug Metadata
For LLVM-based code generation (`LLVMEmitter`), debug emission produces standard LLVM debug metadata:
- `!llvm.dbg.cu = !{!0}`: Declares the DICompileUnit.
- `!DICompileUnit(language: DW_LANG_C, file: !1, producer: "SantientPHP Compiler (sphc)", ...)`: Module-level debug compilation unit descriptor.
- `!DIFile(filename: "...", directory: "...")`: File descriptor metadata.
- Instruction annotations: `; !dbg <fileId>:<line>:<col>` accompanying emitted LLVM IR instructions.

#### 11.8.6 Linker Compatibility & Debugger Verification
- **Linker Configuration**: When debug mode is active, `Linker` passes `-g` to GCC/MinGW and omits the symbol stripping flag `-s`, preserving:
  - `.debug_line`: DWARF line number matrix for stepping and breakpoints.
  - `.debug_info`: Compilation unit and symbol information.
  - `.debug_abbrev` / `.debug_aranges`: DWARF table abbreviations and address range descriptors.
  - `.debug_frame` / `.eh_frame`: Call frame unwinding tables.
- **GDB/LLDB Compatibility**:
  - Setting breakpoints by function name (`break main`, `break compute_sum`) resolves directly to the `.sph` source line.
  - Line stepping (`step`, `next`) navigates SPH statements with source code display.
  - Call stack backtraces (`backtrace`, `bt`) display full function call chains with exact source file names and line numbers.

### 11.9 Cross-Compilation Support & Target Triples

#### 11.9.1 Supported Target Architectures and Platforms
The SPH compiler (`sphc`) provides first-class target triple abstraction and cross-compilation infrastructure across major 64-bit architectures and operating systems:

| Target Triple | Canonical Triple | Architecture | Operating System | ABI / Environment | Calling Convention |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `x86_64-windows` | `x86_64-w64-windows-gnu` | x86-64 | Windows | MinGW / GNU ABI | Win64 (`rcx, rdx, r8, r9`, shadow space) |
| `x86_64-linux` | `x86_64-unknown-linux-gnu` | x86-64 | Linux | System V ELF / GNU | SysV64 (`rdi, rsi, rdx, rcx, r8, r9`) |
| `aarch64-linux` | `aarch64-unknown-linux-gnu` | AArch64 | Linux | GNU ABI | AAPCS64 (`x0-x7`, `d0-d7`) |
| `aarch64-macos` | `arm64-apple-macosx` | AArch64 | Apple macOS | Darwin / Mach-O | AAPCS64 (`x0-x7`, `d0-d7`) |
| `x86_64-macos` | `x86_64-apple-darwin` | x86-64 | Apple macOS | Darwin / Mach-O | SysV64 (`rdi, rsi, rdx, rcx, r8, r9`) |
| `aarch64-windows` | `aarch64-pc-windows-msvc` | AArch64 | Windows | MSVC ABI | AAPCS64 |

#### 11.9.2 Target Configuration & Inspection
- **CLI Options**:
  - `-t <triple>`, `--target=<triple>`, `--target <triple>`: Configures the target for compilation, assembly emission, or LLVM generation.
  - `targets`, `--list-targets`: Displays a formatted matrix of all supported target platforms, canonical triples, descriptions, and highlights the host platform.
- **Manifest Configuration (`sph.toml`)**:
  ```toml
  [target]
  triple = "x86_64-linux"
  ```
  Specifying `triple` in `sph.toml` automatically selects the target when running `sphc build`, `sphc emit-asm`, or `sphc emit-llvm`.

#### 11.9.3 Native Code Generator ABI Adaptation
The native machine code generator (`X86_64CodeGen`) dynamically adapts its code emission based on the target platform:
1. **Calling Conventions**:
   - **Win64 ABI** (`x86_64-windows`):
     - Uses 4 register arguments (`rcx, rdx, r8, r9` for integers, `xmm0-xmm3` for floats).
     - Allocates 32-byte shadow space in caller stack frame preceding outgoing calls.
     - Stack parameters beyond 4 are placed at `[rsp + 32 + (i-4)*8]`.
   - **System V AMD64 ABI** (`x86_64-linux`, `x86_64-macos`):
     - Uses 6 register arguments (`rdi, rsi, rdx, rcx, r8, r9` for integers, `xmm0-xmm7` for floats).
     - Does not allocate shadow space (0-byte overhead).
     - Stack parameters beyond 6 are placed at `[rsp + (i-6)*8]`.
     - 16-byte stack frame alignment maintained before calls.
2. **Object and Data Sections**:
   - Windows PE/COFF: Read-only data is emitted to `.section .rdata,"dr"`.
   - Linux ELF: Read-only data is emitted to `.section .rodata`.
   - macOS Mach-O: Read-only string literals are emitted to `.section __TEXT,__cstring,cstring_literals`.
3. **Symbol Metadata**:
   - Windows PE/COFF: Emits `.def <name>; .scl 2; .type 32; .endef`.
   - Linux ELF: Emits `.type <name>, @function`.

#### 11.9.4 LLVM Data Layout & Triple Specification
When emitting LLVM IR (`LLVMEmitter`), target metadata is automatically derived and emitted:
- `target datalayout = "..."`: Exactly specifies endianness, pointer size (64-bit), and type alignments for the target platform.
- `target triple = "..."`: Emits the canonical target triple recognized by Clang and LLVM backend pipelines.

#### 11.9.5 Toolchain Abstraction & Cross-Linking
The `Linker` subsystem abstracts host and cross-toolchain discovery:
- **Host Compilation**: When targeting the host (e.g., `x86_64-windows` on Windows), invokes host GCC/MinGW to link executables directly.
- **Cross-Compilation**: When cross-compiling (e.g., targeting `x86_64-linux` from Windows):
  - Automatically queries PATH for cross-compilers (`x86_64-linux-gnu-gcc`, `aarch64-linux-gnu-gcc`, or `clang --target=<triple>`).
  - If a cross-compiler is available, links the cross-binary directly.
  - If a cross-compiler is not present, `sphc` generates the clean, verified target assembly file (`.s`) or LLVM bitcode (`.ll`) and outputs actionable guidance with the exact linker command to complete binary generation on the target system or container.














