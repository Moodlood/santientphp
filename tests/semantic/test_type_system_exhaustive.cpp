#include "common/test_framework.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <string>

namespace {

bool checkValid(const std::string& code) {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    uint32_t fileId = sm.addSource("type_test.sph", code);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program || emitter.hasErrors()) {
        return false;
    }
    sph::SemanticAnalyzer analyzer(emitter);
    return analyzer.analyze(*program) && !emitter.hasErrors();
}

bool checkDynamicValid(const std::string& code) {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    uint32_t fileId = sm.addSource("type_test.sph", code);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program || emitter.hasErrors()) {
        return false;
    }
    sph::SemanticAnalyzer analyzer(emitter);
    analyzer.setDynamicMode(true);
    return analyzer.analyze(*program) && !emitter.hasErrors();
}

bool checkInvalid(const std::string& code, const std::string& expectedErrorCode = "") {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    uint32_t fileId = sm.addSource("type_test.sph", code);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program || emitter.hasErrors()) {
        return true;
    }
    sph::SemanticAnalyzer analyzer(emitter);
    bool ok = analyzer.analyze(*program);
    if (!ok || emitter.hasErrors()) {
        if (!expectedErrorCode.empty()) {
            for (const auto& d : emitter.getDiagnostics()) {
                if (d.code == expectedErrorCode || d.message.find(expectedErrorCode) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }
        return true;
    }
    return false;
}

} // namespace

// =============================================================================
// Type System Exhaustive: Primitives & Conversions (301 to 340)
// =============================================================================

TEST_CASE("TypeSystemExhaustive: 301 i32 literal assignment") {
    CHECK(checkValid("<?php fn test(): i32 { let x: i32 = 42; return x; }"));
}

TEST_CASE("TypeSystemExhaustive: 302 i64 literal assignment") {
    CHECK(checkValid("<?php fn test(): i64 { let x: i64 = 1000000 as i64; return x; }"));
}

TEST_CASE("TypeSystemExhaustive: 303 f32 literal assignment") {
    CHECK(checkValid("<?php fn test(): f32 { let x: f32 = 3.14 as f32; return x; }"));
}

TEST_CASE("TypeSystemExhaustive: 304 f64 literal assignment") {
    CHECK(checkValid("<?php fn test(): f64 { let x: f64 = 2.718281828; return x; }"));
}

TEST_CASE("TypeSystemExhaustive: 305 bool true assignment") {
    CHECK(checkValid("<?php fn test(): bool { let x: bool = true; return x; }"));
}

TEST_CASE("TypeSystemExhaustive: 306 bool false assignment") {
    CHECK(checkValid("<?php fn test(): bool { let x: bool = false; return x; }"));
}

TEST_CASE("TypeSystemExhaustive: 307 string literal assignment") {
    CHECK(checkValid("<?php fn test(): string { let x: string = \"hello\"; return x; }"));
}

TEST_CASE("TypeSystemExhaustive: 308 Cast i32 to i64") {
    CHECK(checkValid("<?php fn test(): i64 { let x: i32 = 42; return x as i64; }"));
}

TEST_CASE("TypeSystemExhaustive: 309 Cast i32 to f64") {
    CHECK(checkValid("<?php fn test(): f64 { let x: i32 = 42; return x as f64; }"));
}

TEST_CASE("TypeSystemExhaustive: 310 Cast f64 to i32") {
    CHECK(checkValid("<?php fn test(): i32 { let x: f64 = 3.99; return x as i32; }"));
}

TEST_CASE("TypeSystemExhaustive: 311 Inferred variable type integer") {
    CHECK(checkValid("<?php fn test(): i32 { let x = 10; return x + 5; }"));
}

TEST_CASE("TypeSystemExhaustive: 312 Inferred variable type boolean") {
    CHECK(checkValid("<?php fn test(): bool { let x = true; return !x; }"));
}

TEST_CASE("TypeSystemExhaustive: 313 Inferred variable type string") {
    CHECK(checkValid("<?php fn test(): string { let x = \"hello\"; return x; }"));
}

TEST_CASE("TypeSystemExhaustive: 314 Reject string assignment to i32") {
    CHECK(checkInvalid("<?php fn test(): void { let x: i32 = \"str\"; }"));
}

TEST_CASE("TypeSystemExhaustive: 315 Reject bool assignment to i32") {
    CHECK(checkInvalid("<?php fn test(): void { let x: i32 = true; }"));
}

TEST_CASE("TypeSystemExhaustive: 316 Reject i32 assignment to string") {
    CHECK(checkInvalid("<?php fn test(): void { let x: string = 123; }"));
}

TEST_CASE("TypeSystemExhaustive: 317 Reject bool assignment to string") {
    CHECK(checkInvalid("<?php fn test(): void { let x: string = false; }"));
}

TEST_CASE("TypeSystemExhaustive: 318 Reject return type mismatch integer to string") {
    CHECK(checkInvalid("<?php fn test(): string { return 42; }"));
}

TEST_CASE("TypeSystemExhaustive: 319 Reject return type mismatch string to bool") {
    CHECK(checkInvalid("<?php fn test(): bool { return \"yes\"; }"));
}

TEST_CASE("TypeSystemExhaustive: 320 Reject void function returning a value") {
    CHECK(checkInvalid("<?php fn test(): void { return 10; }"));
}

TEST_CASE("TypeSystemExhaustive: 321 Reject non-void function returning nothing") {
    CHECK(checkInvalid("<?php fn test(): i32 { return; }"));
}

TEST_CASE("TypeSystemExhaustive: 322 Binary arithmetic on integers produces integer") {
    CHECK(checkValid("<?php fn test(): i32 { return (10 + 5) * (8 - 2) / 3 % 4; }"));
}

TEST_CASE("TypeSystemExhaustive: 323 Comparison of integers produces bool") {
    CHECK(checkValid("<?php fn test(): bool { return (10 > 5) && (2 <= 3) && (4 == 4) && (1 != 2); }"));
}

TEST_CASE("TypeSystemExhaustive: 324 Logical operations produce bool") {
    CHECK(checkValid("<?php fn test(): bool { return true && !false || false; }"));
}

TEST_CASE("TypeSystemExhaustive: 325 Bitwise operations on integers produce integer") {
    CHECK(checkValid("<?php fn test(): i32 { return (0x0F & 0xFF) | (~0xAA ^ 0x55) << 2 >> 1; }"));
}

TEST_CASE("TypeSystemExhaustive: 326 Reject logical AND on integers") {
    CHECK(checkInvalid("<?php fn test(): bool { return 1 && 2; }"));
}

TEST_CASE("TypeSystemExhaustive: 327 Reject logical OR on strings") {
    CHECK(checkInvalid("<?php fn test(): bool { return \"a\" || \"b\"; }"));
}

TEST_CASE("TypeSystemExhaustive: 328 Reject addition of string and integer without concat") {
    CHECK(checkInvalid("<?php fn test(): void { let x = \"num: \" + 10; }"));
}

TEST_CASE("TypeSystemExhaustive: 329 String concatenation with dot") {
    CHECK(checkValid("<?php fn test(): string { return \"a\" . \"b\" . \"c\"; }"));
}

TEST_CASE("TypeSystemExhaustive: 330 Reject if condition non-bool integer") {
    CHECK(checkInvalid("<?php fn test(): void { if (1) {} }"));
}

TEST_CASE("TypeSystemExhaustive: 331 Reject if condition non-bool string") {
    CHECK(checkInvalid("<?php fn test(): void { if (\"true\") {} }"));
}

TEST_CASE("TypeSystemExhaustive: 332 Reject while condition non-bool integer") {
    CHECK(checkInvalid("<?php fn test(): void { while (1) {} }"));
}

TEST_CASE("TypeSystemExhaustive: 333 Valid if condition bool expression") {
    CHECK(checkValid("<?php fn test(x: i32): i32 { if (x > 0) { return 1; } else { return -1; } }"));
}

TEST_CASE("TypeSystemExhaustive: 334 Valid while loop with bool expression") {
    CHECK(checkValid("<?php fn test(): void { mut x = 0; while (x < 10) { x = x + 1; } }"));
}

TEST_CASE("TypeSystemExhaustive: 335 Function call argument count check exact match") {
    CHECK(checkValid("<?php fn add(a: i32, b: i32): i32 { return a + b; } fn test(): i32 { return add(1, 2); }"));
}

TEST_CASE("TypeSystemExhaustive: 336 Function call argument count check too few") {
    CHECK(checkInvalid("<?php fn add(a: i32, b: i32): i32 { return a + b; } fn test(): i32 { return add(1); }"));
}

TEST_CASE("TypeSystemExhaustive: 337 Function call argument count check too many") {
    CHECK(checkInvalid("<?php fn add(a: i32, b: i32): i32 { return a + b; } fn test(): i32 { return add(1, 2, 3); }"));
}

TEST_CASE("TypeSystemExhaustive: 338 Function call argument type mismatch") {
    CHECK(checkInvalid("<?php fn add(a: i32, b: i32): i32 { return a + b; } fn test(): i32 { return add(1, \"two\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 339 Recursive function call type check") {
    CHECK(checkValid("<?php fn fact(n: i32): i32 { if (n <= 1) { return 1; } return n * fact(n - 1); }"));
}

TEST_CASE("TypeSystemExhaustive: 340 Mutually recursive functions") {
    CHECK(checkValid("<?php fn is_even(n: i32): bool { if (n == 0) { return true; } return is_odd(n - 1); } fn is_odd(n: i32): bool { if (n == 0) { return false; } return is_even(n - 1); }"));
}

// =============================================================================
// Mutability and Scope Rules (341 to 370)
// =============================================================================

TEST_CASE("TypeSystemExhaustive: 341 Reassignment to let binding rejected") {
    CHECK(checkInvalid("<?php fn test(): void { let x = 10; x = 20; }"));
}

TEST_CASE("TypeSystemExhaustive: 342 Reassignment to mut binding allowed") {
    CHECK(checkValid("<?php fn test(): void { mut x = 10; x = 20; }"));
}

TEST_CASE("TypeSystemExhaustive: 343 Reassignment to mut binding with wrong type rejected") {
    CHECK(checkInvalid("<?php fn test(): void { mut x = 10; x = \"str\"; }"));
}

TEST_CASE("TypeSystemExhaustive: 344 Undeclared variable rejected") {
    CHECK(checkInvalid("<?php fn test(): void { let a = undeclared_var; }"));
}

TEST_CASE("TypeSystemExhaustive: 345 Duplicate variable in same scope rejected") {
    CHECK(checkInvalid("<?php fn test(): void { let x = 1; let x = 2; }"));
}

TEST_CASE("TypeSystemExhaustive: 346 Variable shadowing in inner block allowed") {
    CHECK(checkValid("<?php fn test(): i32 { let x = 10; { let x = \"hello\"; } return x; }"));
}

TEST_CASE("TypeSystemExhaustive: 347 Inner block variable not accessible in outer block") {
    CHECK(checkInvalid("<?php fn test(): void { { let inside = 42; } let outside = inside; }"));
}

TEST_CASE("TypeSystemExhaustive: 348 Function parameter immutable by default") {
    CHECK(checkInvalid("<?php fn test(x: i32): void { x = 20; }"));
}

TEST_CASE("TypeSystemExhaustive: 349 Function parameter mutable with mut keyword") {
    CHECK(checkValid("<?php fn test(&x: i32): void { x = 20; }"));
}

TEST_CASE("TypeSystemExhaustive: 350 Shadowing parameter in function body allowed") {
    CHECK(checkValid("<?php fn test(x: i32): i32 { { let x = 100; return x; } return x; }"));
}

TEST_CASE("TypeSystemExhaustive: 351 Multiple parameters with distinct names") {
    CHECK(checkValid("<?php fn test(a: i32, b: string, c: bool): void {}"));
}

TEST_CASE("TypeSystemExhaustive: 352 Duplicate parameter names rejected") {
    CHECK(checkInvalid("<?php fn test(a: i32, a: i32): void {}"));
}

TEST_CASE("TypeSystemExhaustive: 353 Unsafe block enables raw pointer dereference") {
    CHECK(checkValid("<?php fn test(p: *i32): i32 { unsafe { return *p; } }"));
}

TEST_CASE("TypeSystemExhaustive: 354 Raw pointer dereference outside unsafe block rejected") {
    CHECK(checkInvalid("<?php fn test(p: *i32): i32 { return *p; }"));
}

TEST_CASE("TypeSystemExhaustive: 355 Address-of local variable in unsafe block") {
    CHECK(checkValid("<?php fn test(): *i32 { unsafe { let x = 10; return &x; } }"));
}

TEST_CASE("TypeSystemExhaustive: 356 Pointer assignment to wrong pointee type rejected") {
    CHECK(checkInvalid("<?php fn test(p: *i32): void { unsafe { *p = \"text\"; } }"));
}

TEST_CASE("TypeSystemExhaustive: 357 Null pointer assignable to raw pointer") {
    CHECK(checkValid("<?php fn test(): bool { let x = 42; unsafe { let p: *i32 = &x; return p == (0 as *i32); } }"));
}

TEST_CASE("TypeSystemExhaustive: 358 Rc::new infers element type") {
    CHECK(checkValid("<?php fn test(): Rc<i32> { return Rc::new(42); }"));
}

TEST_CASE("TypeSystemExhaustive: 359 Rc::count returns i64") {
    CHECK(checkValid("<?php fn test(r: Rc<i32>): i64 { return Rc::count(r); }"));
}

TEST_CASE("TypeSystemExhaustive: 360 Box::new infers element type") {
    CHECK(checkValid("<?php fn test(): Box<string> { return Box::new(\"contained\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 361 Arena::new and Arena::alloc pointer type") {
    CHECK(checkValid("<?php struct Node { val: i32 } fn test(a: Arena): *Node { return Arena::alloc(a, Node { val: 100 }); }"));
}

TEST_CASE("TypeSystemExhaustive: 362 Vec<i32> type checking") {
    CHECK(checkValid("<?php fn test(): Vec<i32> { let mut v: Vec<i32> = Vec::new(); v.push(1); v.push(2); return v; }"));
}

TEST_CASE("TypeSystemExhaustive: 363 Vec::push type mismatch rejected") {
    CHECK(checkInvalid("<?php fn test(v: Vec<i32>): void { v.push(\"err\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 364 Defer with valid void expression") {
    CHECK(checkValid("<?php fn clean(): void {} fn test(): void { defer clean(); }"));
}

TEST_CASE("TypeSystemExhaustive: 365 Reject duplicate function declaration in same file") {
    CHECK(checkInvalid("<?php fn foo(): void {} fn foo(): void {}"));
}

TEST_CASE("TypeSystemExhaustive: 366 Reject calling undeclared function") {
    CHECK(checkInvalid("<?php fn test(): void { nonExistentFunction(); }"));
}

TEST_CASE("TypeSystemExhaustive: 367 Forward declaration of function allowed") {
    CHECK(checkValid("<?php fn a(): void { b(); } fn b(): void {}"));
}

TEST_CASE("TypeSystemExhaustive: 368 Ternary branches must have matching types") {
    CHECK(checkValid("<?php fn test(cond: bool): i32 { if (cond) { return 10; } return 20; }"));
}

TEST_CASE("TypeSystemExhaustive: 369 Ternary branches mismatch rejected") {
    CHECK(checkInvalid("<?php fn test(cond: bool): i32 { if (cond) { return 10; } return \"text\"; }"));
}

TEST_CASE("TypeSystemExhaustive: 370 Fixed size array type checking") {
    CHECK(checkValid("<?php fn test(): [i32; 3] { return [1, 2, 3]; }"));
}

// =============================================================================
// Structs, Classes, and Interfaces (371 to 410)
// =============================================================================

TEST_CASE("TypeSystemExhaustive: 371 Struct field access type checking") {
    CHECK(checkValid("<?php struct User { id: i32, name: string } fn get_id(u: User): i32 { return u.id; }"));
}

TEST_CASE("TypeSystemExhaustive: 372 Struct field access non-existent field rejected") {
    CHECK(checkInvalid("<?php struct User { id: i32 } fn test(u: User): void { let x = u.nonExistent; }"));
}

TEST_CASE("TypeSystemExhaustive: 373 Struct init missing required field rejected") {
    CHECK(checkInvalid("<?php struct User { id: i32, name: string } fn test(): User { return User { id: 1 }; }"));
}

TEST_CASE("TypeSystemExhaustive: 374 Struct init field type mismatch rejected") {
    CHECK(checkInvalid("<?php struct User { id: i32 } fn test(): User { return User { id: \"bad\" }; }"));
}

TEST_CASE("TypeSystemExhaustive: 375 Generic struct instantiation with i32") {
    CHECK(checkValid("<?php struct Container<T> { item: T } fn test(): Container<i32> { return Container<i32> { item: 42 }; }"));
}

TEST_CASE("TypeSystemExhaustive: 376 Generic struct instantiation with string") {
    CHECK(checkValid("<?php struct Container<T> { item: T } fn test(): Container<string> { return Container<string> { item: \"hello\" }; }"));
}

TEST_CASE("TypeSystemExhaustive: 377 Generic struct type mismatch rejected") {
    CHECK(checkInvalid("<?php struct Container<T> { item: T } fn test(): Container<i32> { return Container<i32> { item: \"err\" }; }"));
}

TEST_CASE("TypeSystemExhaustive: 378 Enum variant access") {
    CHECK(checkValid("<?php enum Status { Active, Inactive } fn test(): Status { return Status::Active; }"));
}

TEST_CASE("TypeSystemExhaustive: 379 Nonexistent enum variant rejected") {
    CHECK(checkInvalid("<?php enum Status { Active } fn test(): void { let s = Status::Unknown; }"));
}

TEST_CASE("TypeSystemExhaustive: 380 Enum equality comparison") {
    CHECK(checkValid("<?php enum Status { Active, Inactive } fn is_active(s: Status): bool { return s == Status::Active; }"));
}

TEST_CASE("TypeSystemExhaustive: 381 Class method call valid") {
    CHECK(checkValid("<?php class Greeter { public function greet(): string { return \"hi\"; } } fn test(g: Greeter): string { return g->greet(); }"));
}

TEST_CASE("TypeSystemExhaustive: 382 Class method call nonexistent method rejected") {
    CHECK(checkInvalid("<?php class Greeter {} fn test(g: Greeter): void { g->unknown(); }"));
}

TEST_CASE("TypeSystemExhaustive: 383 Class implementing interface satisfies signatures") {
    CHECK(checkValid("<?php interface Reader { fn read(): string; } class FileReader implements Reader { public function read(): string { return \"data\"; } }"));
}

TEST_CASE("TypeSystemExhaustive: 384 Class missing interface method rejected") {
    CHECK(checkInvalid("<?php interface Reader { fn read(): string; } class BadReader implements Reader {}"));
}

TEST_CASE("TypeSystemExhaustive: 385 Class interface method signature return mismatch rejected") {
    CHECK(checkInvalid("<?php interface Reader { fn read(): string; } class BadReader implements Reader { public function read(): i32 { return 42; } }"));
}

TEST_CASE("TypeSystemExhaustive: 386 Dynamic mode variable without type annotation") {
    CHECK(checkDynamicValid("<?php fn main(): void { $x = 10; $x = \"reassigned in dynamic\"; }"));
}

TEST_CASE("TypeSystemExhaustive: 387 Dynamic mode function call") {
    CHECK(checkDynamicValid("<?php function add($a, $b) { return $a + $b; } fn main(): void { $res = add(1, 2); }"));
}

TEST_CASE("TypeSystemExhaustive: 388 Dynamic array append $arr[] = val") {
    CHECK(checkDynamicValid("<?php fn main(): void { $arr = []; $arr[] = 1; $arr[] = \"two\"; }"));
}

TEST_CASE("TypeSystemExhaustive: 389 Dynamic associative array lookup") {
    CHECK(checkDynamicValid("<?php fn main(): void { $map = [\"key\" => \"val\"]; $v = $map[\"key\"]; }"));
}

TEST_CASE("TypeSystemExhaustive: 390 PHP compatibility built-in strlen") {
    CHECK(checkValid("<?php fn test(): i64 { return strlen(\"hello\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 391 PHP compatibility built-in substr") {
    CHECK(checkValid("<?php fn test(): string { return substr(\"hello\", 0, 2); }"));
}

TEST_CASE("TypeSystemExhaustive: 392 PHP compatibility built-in in_array") {
    CHECK(checkValid("<?php fn test(): bool { return in_array(1, [1, 2, 3]); }"));
}

TEST_CASE("TypeSystemExhaustive: 393 PHP compatibility built-in array_keys") {
    CHECK(checkDynamicValid("<?php fn main(): void { $keys = array_keys([\"a\" => 1]); }"));
}

TEST_CASE("TypeSystemExhaustive: 394 PHP compatibility built-in array_values") {
    CHECK(checkDynamicValid("<?php fn main(): void { $vals = array_values([\"a\" => 1]); }"));
}

TEST_CASE("TypeSystemExhaustive: 395 File::exists built-in check") {
    CHECK(checkValid("<?php fn test(): bool { return File::exists(\"test.txt\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 396 File::read built-in check") {
    CHECK(checkValid("<?php fn test(): string { return File::read(\"test.txt\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 397 File::write built-in check") {
    CHECK(checkValid("<?php fn test(): bool { return File::write(\"test.txt\", \"data\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 398 Slice indexing produces element type") {
    CHECK(checkValid("<?php fn test(s: &[i32]): i32 { return s[0]; }"));
}

TEST_CASE("TypeSystemExhaustive: 399 Fixed array indexing produces element type") {
    CHECK(checkValid("<?php fn test(arr: [i32; 4]): i32 { return arr[1]; }"));
}

TEST_CASE("TypeSystemExhaustive: 400 Reject non-integer array index") {
    CHECK(checkInvalid("<?php fn test(arr: [i32; 4]): void { let x = arr[\"bad\"]; }"));
}

TEST_CASE("TypeSystemExhaustive: 401 Reject indexing non-indexable type integer") {
    CHECK(checkInvalid("<?php fn test(): void { let x = 42; let y = x[0]; }"));
}

TEST_CASE("TypeSystemExhaustive: 402 Reject indexing non-indexable type bool") {
    CHECK(checkInvalid("<?php fn test(): void { let x = true; let y = x[0]; }"));
}

TEST_CASE("TypeSystemExhaustive: 403 SIMD f32x4 indexing produces f32") {
    CHECK(checkValid("<?php fn test(v: f32x4): f32 { return v[0]; }"));
}

TEST_CASE("TypeSystemExhaustive: 404 SIMD i32x4 indexing produces i32") {
    CHECK(checkValid("<?php fn test(v: i32x4): i32 { return v[0]; }"));
}

TEST_CASE("TypeSystemExhaustive: 405 Method call on struct") {
    CHECK(checkValid("<?php class Point { public $x: i32; public $y: i32; public function len(): i32 { return $this->x + $this->y; } } fn test(p: Point): i32 { return p->len(); }"));
}

TEST_CASE("TypeSystemExhaustive: 406 Struct with multiple methods") {
    CHECK(checkValid("<?php class Counter { public $val: i32; public function get(): i32 { return $this->val; } public function is_zero(): bool { return $this->val == 0; } }"));
}

TEST_CASE("TypeSystemExhaustive: 407 Function return struct instance") {
    CHECK(checkValid("<?php struct Rect { w: i32, h: i32 } fn make_rect(): Rect { return Rect { w: 10, h: 20 }; }"));
}

TEST_CASE("TypeSystemExhaustive: 408 Pass struct by value to function") {
    CHECK(checkValid("<?php struct Rect { w: i32, h: i32 } fn area(r: Rect): i32 { return r.w * r.h; }"));
}

TEST_CASE("TypeSystemExhaustive: 409 Nested struct access field of field") {
    CHECK(checkValid("<?php struct A { val: i32 } struct B { a: A } fn test(b: B): i32 { return b.a.val; }"));
}

TEST_CASE("TypeSystemExhaustive: 410 Three-level nested struct access") {
    CHECK(checkValid("<?php struct A { val: i32 } struct B { a: A } struct C { b: B } fn test(c: C): i32 { return c.b.a.val; }"));
}

// =============================================================================
// Option, Result, and Match Semantics (411 to 450)
// =============================================================================

TEST_CASE("TypeSystemExhaustive: 411 Some constructor type matching") {
    CHECK(checkValid("<?php fn test(): Option<i32> { return Some(123); }"));
}

TEST_CASE("TypeSystemExhaustive: 412 None constructor assignable to Option<i32>") {
    CHECK(checkValid("<?php fn test(): Option<i32> { return None; }"));
}

TEST_CASE("TypeSystemExhaustive: 413 None constructor assignable to Option<string>") {
    CHECK(checkValid("<?php fn test(): Option<string> { return None; }"));
}

TEST_CASE("TypeSystemExhaustive: 414 Option::none() helper assignable") {
    CHECK(checkValid("<?php fn test(): Option<f64> { return Option::none(); }"));
}

TEST_CASE("TypeSystemExhaustive: 415 Option::some(v) helper assignable") {
    CHECK(checkValid("<?php fn test(): Option<string> { return Option::some(\"ok\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 416 Reject Some with wrong value type") {
    CHECK(checkInvalid("<?php fn test(): Option<i32> { return Some(\"str\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 417 Nested Option<Option<string>> Some(Some(\"text\"))") {
    CHECK(checkValid("<?php fn test(): Option<Option<string>> { return Some(Some(\"text\")); }"));
}

TEST_CASE("TypeSystemExhaustive: 418 Nested Option<Option<string>> Some(None)") {
    CHECK(checkValid("<?php fn test(): Option<Option<string>> { return Some(None); }"));
}

TEST_CASE("TypeSystemExhaustive: 419 Result Ok return matching types") {
    CHECK(checkValid("<?php fn test(): Result<i32, string> { return Ok(10); }"));
}

TEST_CASE("TypeSystemExhaustive: 420 Result Err return matching types") {
    CHECK(checkValid("<?php fn test(): Result<i32, string> { return Err(\"fail\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 421 Result::ok helper") {
    CHECK(checkValid("<?php fn test(): Result<i32, string> { return Result::ok(10); }"));
}

TEST_CASE("TypeSystemExhaustive: 422 Result::err helper") {
    CHECK(checkValid("<?php fn test(): Result<i32, string> { return Result::err(\"error\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 423 Reject Ok payload type mismatch") {
    CHECK(checkInvalid("<?php fn test(): Result<i32, string> { return Ok(\"wrong\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 424 Reject Err payload type mismatch") {
    CHECK(checkInvalid("<?php fn test(): Result<i32, string> { return Err(404); }"));
}

TEST_CASE("TypeSystemExhaustive: 425 Try operator propagates Result in Result function") {
    CHECK(checkValid("<?php fn inner(): Result<i32, string> { return Ok(42); } fn outer(): Result<i32, string> { let x = inner()?; return Ok(x * 2); }"));
}

TEST_CASE("TypeSystemExhaustive: 426 Try operator propagates Option in Option function") {
    CHECK(checkValid("<?php fn inner(): Option<i32> { return Some(42); } fn outer(): Option<i32> { let x = inner()?; return Some(x + 1); }"));
}

TEST_CASE("TypeSystemExhaustive: 427 Reject try operator on non-Result non-Option") {
    CHECK(checkInvalid("<?php fn test(): Result<i32, string> { let x: i32 = 42; let y = x?; return Ok(y); }", "E0277"));
}

TEST_CASE("TypeSystemExhaustive: 428 Reject try operator on Result in non-Result function") {
    CHECK(checkInvalid("<?php fn inner(): Result<i32, string> { return Ok(1); } fn outer(): i32 { let x = inner()?; return x; }", "E0277"));
}

TEST_CASE("TypeSystemExhaustive: 429 Reject try operator on Option in non-Option function") {
    CHECK(checkInvalid("<?php fn inner(): Option<i32> { return Some(1); } fn outer(): i32 { let x = inner()?; return x; }", "E0277"));
}

TEST_CASE("TypeSystemExhaustive: 430 Match arm type consistency") {
    CHECK(checkValid("<?php fn test(x: i32): string { return match (x) { 1 => \"one\", 2 => \"two\", _ => \"other\" }; }"));
}

TEST_CASE("TypeSystemExhaustive: 431 Reject match arm return type inconsistency") {
    CHECK(checkInvalid("<?php fn test(x: i32): string { return match (x) { 1 => \"one\", 2 => 42, _ => \"other\" }; }"));
}

TEST_CASE("TypeSystemExhaustive: 432 Match on Option pattern binds inner value") {
    CHECK(checkValid("<?php fn unwrap_or(opt: Option<i32>, def: i32): i32 { return match (opt) { Some(v) => v, None => def }; }"));
}

TEST_CASE("TypeSystemExhaustive: 433 Match on Result pattern binds Ok and Err values") {
    CHECK(checkValid("<?php fn handle(res: Result<i32, string>): i32 { return match (res) { Ok(v) => v, Err(e) => 0 }; }"));
}

TEST_CASE("TypeSystemExhaustive: 434 Match arm guard condition must be bool") {
    CHECK(checkValid("<?php fn test(x: i32): i32 { return match (x) { n if n > 0 => n, _ => 0 }; }"));
}

TEST_CASE("TypeSystemExhaustive: 435 Reject match arm guard condition non-bool") {
    CHECK(checkInvalid("<?php fn test(x: i32): i32 { return match (x) { n if 42 => n, _ => 0 }; }"));
}

TEST_CASE("TypeSystemExhaustive: 436 Match arm integer range pattern") {
    CHECK(checkValid("<?php fn classify(score: i32): string { return match (score) { 90..100 => \"A\", 80..89 => \"B\", _ => \"C\" }; }"));
}

TEST_CASE("TypeSystemExhaustive: 437 Match arm multiple literal values with pipe") {
    CHECK(checkValid("<?php fn is_vowel(c: i32): bool { return match (c) { 65 | 69 | 73 | 79 | 85 => true, _ => false }; }"));
}

TEST_CASE("TypeSystemExhaustive: 438 Option containing struct") {
    CHECK(checkValid("<?php struct User { id: i32 } fn find(): Option<User> { return Some(User { id: 1 }); }"));
}

TEST_CASE("TypeSystemExhaustive: 439 Result containing struct") {
    CHECK(checkValid("<?php struct Config { port: i32 } fn load(): Result<Config, string> { return Ok(Config { port: 8080 }); }"));
}

TEST_CASE("TypeSystemExhaustive: 440 Vec of structs") {
    CHECK(checkValid("<?php struct Point { x: i32, y: i32 } fn test(): Vec<Point> { let mut v: Vec<Point> = Vec::new(); v.push(Point { x: 0, y: 0 }); return v; }"));
}

TEST_CASE("TypeSystemExhaustive: 441 Struct with Option and Result fields") {
    CHECK(checkValid("<?php struct Response { data: Option<string>, status: Result<i32, string> }"));
}

TEST_CASE("TypeSystemExhaustive: 442 Nested match expressions") {
    CHECK(checkValid("<?php fn test(a: i32, b: i32): i32 { return match (a) { 1 => match (b) { 1 => 11, _ => 10 }, _ => 0 }; }"));
}

TEST_CASE("TypeSystemExhaustive: 443 Match expression result assigned to let binding") {
    CHECK(checkValid("<?php fn test(x: i32): void { let desc = match (x) { 0 => \"zero\", _ => \"non-zero\" }; }"));
}

TEST_CASE("TypeSystemExhaustive: 444 Match on bool true and false") {
    CHECK(checkValid("<?php fn test(b: bool): string { return match (b) { true => \"yes\", false => \"no\" }; }"));
}

TEST_CASE("TypeSystemExhaustive: 445 Match on negative integer literals") {
    CHECK(checkValid("<?php fn test(x: i32): string { return match (x) { -1 => \"negative one\", 0 => \"zero\", 1 => \"one\", _ => \"other\" }; }"));
}

TEST_CASE("TypeSystemExhaustive: 446 Match on string literals") {
    CHECK(checkValid("<?php fn test(s: string): i32 { return match (s) { \"start\" => 1, \"stop\" => 2, _ => 0 }; }"));
}

TEST_CASE("TypeSystemExhaustive: 447 Generic identity function returns same type") {
    CHECK(checkValid("<?php fn id<T>(x: T): T { return x; } fn test(): i32 { return id<i32>(42); }"));
}

TEST_CASE("TypeSystemExhaustive: 448 Generic identity function with string") {
    CHECK(checkValid("<?php fn id<T>(x: T): T { return x; } fn test(): string { return id<string>(\"hello\"); }"));
}

TEST_CASE("TypeSystemExhaustive: 449 Option as function parameter") {
    CHECK(checkValid("<?php fn inspect(opt: Option<i32>): i32 { return match (opt) { Some(v) => v, None => 0 }; } fn test(): i32 { return inspect(Some(99)); }"));
}

TEST_CASE("TypeSystemExhaustive: 450 Result as function parameter") {
    CHECK(checkValid("<?php fn check(res: Result<i32, string>): bool { return match (res) { Ok(v) => true, Err(e) => false }; } fn test(): bool { return check(Ok(1)); }"));
}

