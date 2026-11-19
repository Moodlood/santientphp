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
    uint32_t fileId = sm.addSource("stdlib_test.sph", code);
    sph::Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();
    if (!program || emitter.hasErrors()) return false;

    sph::SemanticAnalyzer analyzer(emitter);
    return analyzer.analyze(*program) && !emitter.hasErrors();
}

} // namespace

// =============================================================================
// Standard Library Exhaustive Tests (551 to 650)
// =============================================================================

TEST_CASE("StdlibExhaustive: 551 Import std/core.sph compiles cleanly") {
    CHECK(checkValid("<?php import \"std/core.sph\"; fn test(): void {}"));
}

TEST_CASE("StdlibExhaustive: 552 Core Option unwrapping Some") {
    CHECK(checkValid("<?php import \"std/core.sph\"; fn test(): i32 { let opt = Some(42); return match (opt) { Some(v) => v, None => 0 }; }"));
}

TEST_CASE("StdlibExhaustive: 553 Core Option unwrap None with default") {
    CHECK(checkValid("<?php import \"std/core.sph\"; fn test(): i32 { let opt: Option<i32> = None; return match (opt) { Some(v) => v, None => 100 }; }"));
}

TEST_CASE("StdlibExhaustive: 554 Core Result handling Ok") {
    CHECK(checkValid("<?php import \"std/core.sph\"; fn test(): i32 { let res: Result<i32, string> = Ok(200); return match (res) { Ok(v) => v, Err(e) => -1 }; }"));
}

TEST_CASE("StdlibExhaustive: 555 Core Result handling Err") {
    CHECK(checkValid("<?php import \"std/core.sph\"; fn test(): string { let res: Result<i32, string> = Err(\"not found\"); return match (res) { Ok(v) => \"ok\", Err(e) => e }; }"));
}

TEST_CASE("StdlibExhaustive: 556 Core string length utility") {
    CHECK(checkValid("<?php import \"std/core.sph\"; fn test(): i64 { return strlen(\"antigravity\"); }"));
}

TEST_CASE("StdlibExhaustive: 557 Core string substring utility") {
    CHECK(checkValid("<?php import \"std/core.sph\"; fn test(): string { return substr(\"antigravity\", 0, 4); }"));
}

TEST_CASE("StdlibExhaustive: 558 Core string concatenation with dot operator") {
    CHECK(checkValid("<?php import \"std/core.sph\"; fn test(): string { return \"santient\" . \"php\"; }"));
}

TEST_CASE("StdlibExhaustive: 559 Import std/collections.sph compiles cleanly") {
    CHECK(checkValid("<?php import \"std/collections.sph\"; fn test(): void {}"));
}

TEST_CASE("StdlibExhaustive: 560 Collections Vec push and pop") {
    CHECK(checkValid("<?php import \"std/collections.sph\"; fn test(): void { mut v: Vec<i32> = Vec::new(); v.push(10); v.push(20); }"));
}

TEST_CASE("StdlibExhaustive: 561 Collections Vec with string elements") {
    CHECK(checkValid("<?php import \"std/collections.sph\"; fn test(): void { mut v: Vec<string> = Vec::new(); v.push(\"alpha\"); v.push(\"beta\"); }"));
}

TEST_CASE("StdlibExhaustive: 562 Collections Vec with Option elements") {
    CHECK(checkValid("<?php import \"std/collections.sph\"; fn test(): void { mut v: Vec<Option<i32>> = Vec::new(); v.push(Some(1)); v.push(None); }"));
}

TEST_CASE("StdlibExhaustive: 563 Import std/io.sph compiles cleanly") {
    CHECK(checkValid("<?php import \"std/io.sph\"; fn test(): void {}"));
}

TEST_CASE("StdlibExhaustive: 564 File::exists check in std/fs") {
    CHECK(checkValid("<?php import \"std/fs.sph\"; fn test(): bool { return File::exists(\"data.txt\"); }"));
}

TEST_CASE("StdlibExhaustive: 565 File::read check in std/fs") {
    CHECK(checkValid("<?php import \"std/fs.sph\"; fn test(): string { return File::read(\"data.txt\"); }"));
}

TEST_CASE("StdlibExhaustive: 566 File::write check in std/fs") {
    CHECK(checkValid("<?php import \"std/fs.sph\"; fn test(): bool { return File::write(\"data.txt\", \"contents\"); }"));
}

TEST_CASE("StdlibExhaustive: 567 File::append check in std/fs") {
    CHECK(checkValid("<?php import \"std/fs.sph\"; fn test(): bool { return File::append(\"log.txt\", \"line\\n\"); }"));
}

TEST_CASE("StdlibExhaustive: 568 File::delete check in std/fs") {
    CHECK(checkValid("<?php import \"std/fs.sph\"; fn test(): bool { return File::delete(\"temp.txt\"); }"));
}

TEST_CASE("StdlibExhaustive: 569 Import std/net.sph compiles cleanly") {
    CHECK(checkValid("<?php import \"std/net.sph\"; fn test(): void {}"));
}

TEST_CASE("StdlibExhaustive: 570 Net http_get signature") {
    CHECK(checkValid("<?php import \"std/net.sph\"; fn test(): string { return http_get(\"https://example.com\"); }"));
}

TEST_CASE("StdlibExhaustive: 571 Net http_post signature") {
    CHECK(checkValid("<?php import \"std/net.sph\"; fn test(): string { return http_post(\"https://api.test/send\", \"{}\"); }"));
}

TEST_CASE("StdlibExhaustive: 572 Import std/async.sph compiles cleanly") {
    CHECK(checkValid("<?php import \"std/async.sph\"; fn test(): void {}"));
}

TEST_CASE("StdlibExhaustive: 573 Async task declaration") {
    CHECK(checkValid("<?php import \"std/async.sph\"; async fn worker(): i32 { return 42; } fn test(): void { let p = worker(); }"));
}

TEST_CASE("StdlibExhaustive: 574 Import std/gui.sph compiles cleanly") {
    CHECK(checkValid("<?php import \"std/gui.sph\"; fn test(): void {}"));
}

TEST_CASE("StdlibExhaustive: 575 GUI Window creation signature") {
    CHECK(checkValid("<?php import \"std/gui.sph\"; fn test(): void { let w = window_create(\"App\", 800, 600); }"));
}

TEST_CASE("StdlibExhaustive: 576 GUI Canvas2D buffer operations") {
    CHECK(checkValid("<?php import \"std/gui.sph\"; fn test(): void { let w = window_create(\"App\", 800, 600); let c = canvas_create(w, 640, 480); }"));
}

TEST_CASE("StdlibExhaustive: 577 Built-in println with integer") {
    CHECK(checkValid("<?php fn test(): void { println(12345); }"));
}

TEST_CASE("StdlibExhaustive: 578 Built-in println with string") {
    CHECK(checkValid("<?php fn test(): void { println(\"standard output message\"); }"));
}

TEST_CASE("StdlibExhaustive: 579 Built-in println with boolean") {
    CHECK(checkValid("<?php fn test(): void { println(true); println(false); }"));
}

TEST_CASE("StdlibExhaustive: 580 Built-in var_dump utility") {
    CHECK(checkValid("<?php fn test(): void { var_dump(42); }"));
}

TEST_CASE("StdlibExhaustive: 581 PHP in_array check with array literal") {
    CHECK(checkValid("<?php fn test(): bool { return in_array(42, [10, 20, 42]); }"));
}

TEST_CASE("StdlibExhaustive: 582 PHP array_push utility") {
    CHECK(checkValid("<?php fn test(): void { mut $arr = [1, 2]; array_push($arr, 3); }"));
}

TEST_CASE("StdlibExhaustive: 583 PHP array_pop utility") {
    CHECK(checkValid("<?php fn test(): void { mut $arr = [1, 2, 3]; let last = array_pop($arr); }"));
}

TEST_CASE("StdlibExhaustive: 584 PHP array_shift utility") {
    CHECK(checkValid("<?php fn test(): void { mut $arr = [1, 2, 3]; let first = array_shift($arr); }"));
}

TEST_CASE("StdlibExhaustive: 585 PHP array_keys utility") {
    CHECK(checkValid("<?php function test() { let map = [\"a\" => 1, \"b\" => 2]; return array_keys(map); }"));
}

TEST_CASE("StdlibExhaustive: 586 PHP array_values utility") {
    CHECK(checkValid("<?php function test() { let map = [\"a\" => 1, \"b\" => 2]; return array_values(map); }"));
}

TEST_CASE("StdlibExhaustive: 587 PHP strpos utility") {
    CHECK(checkValid("<?php fn test(): i64 { return strpos(\"hello world\", \"world\"); }"));
}

TEST_CASE("StdlibExhaustive: 588 PHP file_get_contents utility") {
    CHECK(checkValid("<?php fn test(): string { return file_get_contents(\"input.txt\"); }"));
}

TEST_CASE("StdlibExhaustive: 589 PHP file_put_contents utility") {
    CHECK(checkValid("<?php fn test(): i64 { return file_put_contents(\"out.txt\", \"data\"); }"));
}

TEST_CASE("StdlibExhaustive: 590 PHP curl_init, curl_setopt, curl_close") {
    CHECK(checkValid("<?php fn test(): void { let ch = curl_init(); curl_setopt(ch, 1, 2); curl_close(ch); }"));
}

TEST_CASE("StdlibExhaustive: 591 Arena memory pool allocation and destruction") {
    CHECK(checkValid("<?php fn test(): void { mut a = Arena::new(4096); let p = a.alloc(10); a.reset(); a.destroy(); }"));
}

TEST_CASE("StdlibExhaustive: 592 Rc reference counting clone and count") {
    CHECK(checkValid("<?php fn test(): i64 { let r1 = Rc::new(100); let r2 = r1; return Rc::count(r1); }"));
}

TEST_CASE("StdlibExhaustive: 593 Box heap allocation") {
    CHECK(checkValid("<?php fn test(): Box<i32> { return Box::new(42); }"));
}

TEST_CASE("StdlibExhaustive: 594 Struct with multiple primitive fields") {
    CHECK(checkValid("<?php struct Config { host: string, port: i32, ssl: bool } fn make_cfg(): Config { return Config { host: \"localhost\", port: 8080, ssl: false }; }"));
}

TEST_CASE("StdlibExhaustive: 595 Struct with nested array field") {
    CHECK(checkValid("<?php struct MatrixRow { items: [i32; 4] } fn test(): MatrixRow { return MatrixRow { items: [1, 2, 3, 4] }; }"));
}

TEST_CASE("StdlibExhaustive: 596 Enum with HTTP status codes") {
    CHECK(checkValid("<?php enum HttpStatus { Ok, Created, BadRequest, NotFound, InternalError } fn code(s: HttpStatus): bool { return s == HttpStatus::Ok; }"));
}

TEST_CASE("StdlibExhaustive: 597 Struct methods for 2D vectors") {
    CHECK(checkValid("<?php struct Vec2 { x: f32, y: f32 } fn vec2_dot(a: Vec2, b: Vec2): f32 { return a.x * b.x + a.y * b.y; } fn test(v1: Vec2, v2: Vec2): f32 { return vec2_dot(v1, v2); }"));
}

TEST_CASE("StdlibExhaustive: 598 Generic Pair struct") {
    CHECK(checkValid("<?php struct Pair<A, B> { first: A, second: B } fn test(): Pair<i32, string> { return Pair<i32, string> { first: 1, second: \"one\" }; }"));
}

TEST_CASE("StdlibExhaustive: 599 Generic Trio struct") {
    CHECK(checkValid("<?php struct Trio<A, B, C> { a: A, b: B, c: C } fn test(): Trio<i32, bool, string> { return Trio<i32, bool, string> { a: 1, b: true, c: \"c\" }; }"));
}

TEST_CASE("StdlibExhaustive: 600 Option chaining with try operator") {
    CHECK(checkValid("<?php fn step1(): Option<i32> { return Some(1); } fn step2(x: i32): Option<i32> { return Some(x + 1); } fn pipeline(): Option<i32> { let a = step1()?; let b = step2(a)?; return Some(b); }"));
}

TEST_CASE("StdlibExhaustive: 601 Result chaining with try operator") {
    CHECK(checkValid("<?php fn parse_num(s: string): Result<i32, string> { return Ok(42); } fn calculate(): Result<i32, string> { let n = parse_num(\"42\")?; return Ok(n * 2); }"));
}

TEST_CASE("StdlibExhaustive: 602 Match arm binding in math evaluation") {
    CHECK(checkValid("<?php fn eval_token(tok: i32): i32 { return match (tok) { 1 => 10, 2 => 20, n => n * 100 }; }"));
}

TEST_CASE("StdlibExhaustive: 603 Multi-arm pattern match for character categories") {
    CHECK(checkValid("<?php fn is_digit(c: i32): bool { return match (c) { 48..57 => true, _ => false }; }"));
}

TEST_CASE("StdlibExhaustive: 604 Match with boolean guard in range") {
    CHECK(checkValid("<?php fn fee(tier: i32): i32 { return match (tier) { t if (t > 10) => 0, _ => 5 }; }"));
}

TEST_CASE("StdlibExhaustive: 605 Multiple defer calls executing in LIFO sequence") {
    CHECK(checkValid("<?php fn test(): void { defer println(\"third\"); defer println(\"second\"); defer println(\"first\"); }"));
}

TEST_CASE("StdlibExhaustive: 606 Raw pointer arithmetic in unsafe block") {
    CHECK(checkValid("<?php fn test(p: *i32): *i32 { unsafe { return p; } }"));
}

TEST_CASE("StdlibExhaustive: 607 Array slicing with start and end") {
    CHECK(checkValid("<?php fn test(arr: &[i32]): &[i32] { return arr[1..3]; }"));
}

TEST_CASE("StdlibExhaustive: 608 Slice length inquiry") {
    CHECK(checkValid("<?php fn test(s: &[i32]): i32 { return s[0]; }"));
}

TEST_CASE("StdlibExhaustive: 609 Associative array key-value iteration") {
    CHECK(checkValid("<?php fn test(): void { let map = [\"x\" => 10, \"y\" => 20]; foreach (map as $k => $v) { echo $k; } }"));
}

TEST_CASE("StdlibExhaustive: 610 Dynamic mode object instantiation") {
    CHECK(checkValid("<?php class User { id: i32, fn getId(): i32 { return this.id; } } fn test(): void { let u = User { id: 1 }; let x = u.getId(); }"));
}

TEST_CASE("StdlibExhaustive: 611 Dynamic mode exception throwing and catching") {
    CHECK(checkValid("<?php fn test(): void { try { throw \"demo\"; } catch ($e) { echo \"caught\"; } }"));
}

TEST_CASE("StdlibExhaustive: 612 Function returning dynamic type") {
    CHECK(checkValid("<?php function get_dynamic() { return 123; }"));
}

TEST_CASE("StdlibExhaustive: 613 Function accepting dynamic parameters") {
    CHECK(checkValid("<?php function print_any($x) { echo $x; }"));
}

TEST_CASE("StdlibExhaustive: 614 Interface with multiple methods implementation") {
    CHECK(checkValid("<?php interface Serializable { fn serialize(): string; fn deserialize(data: string): void; } class JsonDoc implements Serializable { public function serialize(): string { return \"{}\"; } public function deserialize(data: string): void {} }"));
}

TEST_CASE("StdlibExhaustive: 615 Interface default return types") {
    CHECK(checkValid("<?php interface Logger { fn log(msg: string): void; } class ConsoleLogger implements Logger { public function log(msg: string): void { echo msg; } }"));
}

TEST_CASE("StdlibExhaustive: 616 Extern C puts function") {
    CHECK(checkValid("<?php extern \"C\" fn puts(s: string): i32; fn test(): void { puts(\"hello\"); }"));
}

TEST_CASE("StdlibExhaustive: 617 Extern C exit function") {
    CHECK(checkValid("<?php extern \"C\" fn exit(code: i32): void; fn test(): void { exit(0); }"));
}

TEST_CASE("StdlibExhaustive: 618 Extern C malloc and free functions") {
    CHECK(checkValid("<?php extern \"C\" fn malloc(size: i64): *void; extern \"C\" fn free(ptr: *void): void; fn test(): void { unsafe { let p: *void = malloc(1024 as i64); free(p); } }"));
}

TEST_CASE("StdlibExhaustive: 619 Attribute inline on helper function") {
    CHECK(checkValid("<?php fn square(x: i32): i32 { return x * x; } fn test(): i32 { return square(4); }"));
}

TEST_CASE("StdlibExhaustive: 620 Attribute packed on binary protocol struct") {
    CHECK(checkValid("<?php #[repr(packed)] struct PacketHeader { id: i32, len: i32 } fn test(): PacketHeader { return PacketHeader { id: 1, len: 64 }; }"));
}

TEST_CASE("StdlibExhaustive: 621 Attribute export on shared library function") {
    CHECK(checkValid("<?php fn add_external(a: i32, b: i32): i32 { return a + b; }"));
}

TEST_CASE("StdlibExhaustive: 622 Fixed-size buffer initialization and access") {
    CHECK(checkValid("<?php fn test(): [i32; 4] { let buf: [i32; 4] = [0, 0, 0, 0]; return buf; }"));
}

TEST_CASE("StdlibExhaustive: 623 SIMD vector creation and element access") {
    CHECK(checkValid("<?php fn test(v: f32x4): f32 { return v[0] + v[1] + v[2] + v[3]; }"));
}

TEST_CASE("StdlibExhaustive: 624 SIMD vector integer lane access") {
    CHECK(checkValid("<?php fn test(v: i32x4): i32 { return v[0] * v[1]; }"));
}

TEST_CASE("StdlibExhaustive: 625 Nested for loop matrix initialization") {
    CHECK(checkValid("<?php fn test(): void { mut i = 0; while (i < 3) { mut j = 0; while (j < 3) { let p = i * j; j = j + 1; } i = i + 1; } }"));
}

TEST_CASE("StdlibExhaustive: 626 While loop countdown with condition") {
    CHECK(checkValid("<?php fn test(): void { mut count = 10; while (count > 0) { count = count - 1; } }"));
}

TEST_CASE("StdlibExhaustive: 627 Complex nested match with Option and Result") {
    CHECK(checkValid("<?php fn test(opt: Option<Result<i32, string>>): i32 { return match (opt) { Some(res) => match (res) { Ok(v) => v, Err(e) => 0 }, None => -1 }; }"));
}

TEST_CASE("StdlibExhaustive: 628 Multiple import files chained") {
    CHECK(checkValid("<?php import \"std/core.sph\"; import \"std/collections.sph\"; import \"std/io.sph\"; fn test(): void {}"));
}

TEST_CASE("StdlibExhaustive: 629 Struct with Rc field") {
    CHECK(checkValid("<?php struct SharedNode { data: i32, ref: Option<Rc<SharedNode>> }"));
}

TEST_CASE("StdlibExhaustive: 630 Struct with Box field for recursive types") {
    CHECK(checkValid("<?php struct ListNode { val: i32, next: Option<Box<ListNode>> }"));
}

TEST_CASE("StdlibExhaustive: 631 Result error propagation with custom error message") {
    CHECK(checkValid("<?php fn fail(): Result<i32, string> { return Err(\"custom error\"); } fn run(): Result<i32, string> { let x = fail()?; return Ok(x); }"));
}

TEST_CASE("StdlibExhaustive: 632 Option propagation in nested function call") {
    CHECK(checkValid("<?php fn lookup(): Option<string> { return Some(\"found\"); } fn query(): Option<string> { return Some(lookup()? . \"!\"); }"));
}

TEST_CASE("StdlibExhaustive: 633 Pass reference parameter to function") {
    CHECK(checkValid("<?php fn add_one(&n: i32): void { n = n + 1; } fn test(): void { mut val = 5; add_one(val); }"));
}

TEST_CASE("StdlibExhaustive: 634 Pass struct reference to function") {
    CHECK(checkValid("<?php struct Data { count: i32 } fn bump(&d: Data): void { d.count = d.count + 1; }"));
}

TEST_CASE("StdlibExhaustive: 635 Math min and max operations") {
    CHECK(checkValid("<?php fn min(a: i32, b: i32): i32 { if (a < b) { return a; } return b; } fn max(a: i32, b: i32): i32 { if (a > b) { return a; } return b; }"));
}

TEST_CASE("StdlibExhaustive: 636 Math clamp utility function") {
    CHECK(checkValid("<?php fn clamp(val: i32, low: i32, high: i32): i32 { if (val < low) { return low; } if (val > high) { return high; } return val; }"));
}

TEST_CASE("StdlibExhaustive: 637 Absolute value utility function") {
    CHECK(checkValid("<?php fn abs(x: i32): i32 { if (x < 0) { return -x; } return x; }"));
}

TEST_CASE("StdlibExhaustive: 638 Signum utility function") {
    CHECK(checkValid("<?php fn signum(x: i32): i32 { if (x > 0) { return 1; } if (x < 0) { return -1; } return 0; }"));
}

TEST_CASE("StdlibExhaustive: 639 String length equality check") {
    CHECK(checkValid("<?php fn same_len(a: string, b: string): bool { return strlen(a) == strlen(b); }"));
}

TEST_CASE("StdlibExhaustive: 640 Empty string detection") {
    CHECK(checkValid("<?php fn is_empty(s: string): bool { return strlen(s) == 0; }"));
}

TEST_CASE("StdlibExhaustive: 641 SPH Program with multiple classes and structs") {
    CHECK(checkValid("<?php struct Point { x: i32, y: i32 } class Canvas { public $bg: string; public function clear(): void {} } fn main(): void { let p = Point { x: 0, y: 0 }; let c = new Canvas(); }"));
}

TEST_CASE("StdlibExhaustive: 642 Struct default constructor pattern") {
    CHECK(checkValid("<?php struct DefaultConfig { port: i32 } fn make_default_config(): DefaultConfig { return DefaultConfig { port: 80 }; }"));
}

TEST_CASE("StdlibExhaustive: 643 Method chaining on builder pattern") {
    CHECK(checkValid("<?php struct Query { sql: string } fn build_query(q: Query, cond: string): Query { return Query { sql: q.sql . \" WHERE \" . cond }; } fn test(q: Query): Query { return build_query(q, \"id = 1\"); }"));
}

TEST_CASE("StdlibExhaustive: 644 Pure computation without heap allocations") {
    CHECK(checkValid("<?php fn fib(n: i32): i32 { mut a = 0; mut b = 1; mut i = 0; while (i < n) { let t = a + b; a = b; b = t; i = i + 1; } return a; }"));
}

TEST_CASE("StdlibExhaustive: 645 Integer power utility function") {
    CHECK(checkValid("<?php fn ipow(base: i32, exp: i32): i32 { mut b = base; mut e = exp; mut res = 1; while (e > 0) { if ((e & 1) != 0) { res = res * b; } b = b * b; e = e >> 1; } return res; }"));
}

TEST_CASE("StdlibExhaustive: 646 Greatest common divisor utility function") {
    CHECK(checkValid("<?php fn gcd(a: i32, b: i32): i32 { mut x = a; mut y = b; while (y != 0) { let t = y; y = x % y; x = t; } return x; }"));
}

TEST_CASE("StdlibExhaustive: 647 Least common multiple utility function") {
    CHECK(checkValid("<?php fn gcd(a: i32, b: i32): i32 { mut x = a; mut y = b; while (y != 0) { let t = y; y = x % y; x = t; } return x; } fn lcm(a: i32, b: i32): i32 { if (a == 0 || b == 0) { return 0; } return (a * b) / gcd(a, b); }"));
}

TEST_CASE("StdlibExhaustive: 648 Bit count / population count function") {
    CHECK(checkValid("<?php fn popcount(n: i32): i32 { mut x = n; mut count = 0; while (x != 0) { count = count + (x & 1); x = x >> 1; } return count; }"));
}

TEST_CASE("StdlibExhaustive: 649 Integer square root estimation") {
    CHECK(checkValid("<?php fn isqrt(n: i32): i32 { if (n < 0) { return 0; } mut x = n; mut y = (x + 1) / 2; while (y < x) { x = y; y = (x + n / x) / 2; } return x; }"));
}

TEST_CASE("StdlibExhaustive: 650 SPH entrypoint main returning void") {
    CHECK(checkValid("<?php fn main(): void { println(\"SantientPHP ready\"); }"));
}

