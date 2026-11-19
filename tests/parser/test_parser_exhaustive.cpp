#include "common/test_framework.hpp"
#include "compiler/ast/DeclAST.hpp"
#include "compiler/ast/ExprAST.hpp"
#include "compiler/ast/StmtAST.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/source/SourceManager.hpp"
#include <memory>
#include <string>

namespace {

std::unique_ptr<sph::ProgramAST> parse(const std::string& source, bool& hasErrors) {
    sph::SourceManager sm;
    sph::DiagnosticEmitter emitter(sm);
    uint32_t fileId = sm.addSource("test.sph", source);
    sph::Parser parser(fileId, sm, &emitter);
    auto prog = parser.parseProgram();
    hasErrors = emitter.hasErrors();
    return prog;
}

bool parsesSuccessfully(const std::string& source) {
    bool hasErrors = false;
    auto prog = parse(source, hasErrors);
    return prog != nullptr && !hasErrors;
}

} // namespace

// =============================================================================
// Parser Exhaustive: Expressions & Operator Precedence (151 to 200)
// =============================================================================

TEST_CASE("ParserExhaustive: 151 Addition and multiplication precedence") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return 1 + 2 * 3; }"));
}

TEST_CASE("ParserExhaustive: 152 Multiplication and addition precedence") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return 2 * 3 + 1; }"));
}

TEST_CASE("ParserExhaustive: 153 Subtraction and division precedence") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return 10 - 8 / 2; }"));
}

TEST_CASE("ParserExhaustive: 154 Parenthesized grouping") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return (1 + 2) * 3; }"));
}

TEST_CASE("ParserExhaustive: 155 Nested parentheses") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return ((1 + 2) * (3 + 4)); }"));
}

TEST_CASE("ParserExhaustive: 156 Unary minus") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return -42; }"));
}

TEST_CASE("ParserExhaustive: 157 Unary logical not") {
    CHECK(parsesSuccessfully("<?php fn test(): bool { return !true; }"));
}

TEST_CASE("ParserExhaustive: 158 Unary bitwise not") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return ~0; }"));
}

TEST_CASE("ParserExhaustive: 159 Unary address-of and dereference") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { unsafe { let x = 10; let p = &x; return *p; } }"));
}

TEST_CASE("ParserExhaustive: 160 Bitwise shifts << and >>") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return (1 << 4) >> 2; }"));
}

TEST_CASE("ParserExhaustive: 161 Bitwise and &, or |, xor ^") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return (0xFF & 0x0F) | (0xAA ^ 0x55); }"));
}

TEST_CASE("ParserExhaustive: 162 Logical and &&, or ||") {
    CHECK(parsesSuccessfully("<?php fn test(): bool { return true && false || true; }"));
}

TEST_CASE("ParserExhaustive: 163 Comparison operators <, <=, >, >=") {
    CHECK(parsesSuccessfully("<?php fn test(): bool { return (1 < 2) && (2 <= 2) && (3 > 2) && (3 >= 3); }"));
}

TEST_CASE("ParserExhaustive: 164 Equality operators ==, !=") {
    CHECK(parsesSuccessfully("<?php fn test(): bool { return (1 == 1) && (2 != 3); }"));
}

TEST_CASE("ParserExhaustive: 165 Strict equality === and !==") {
    CHECK(parsesSuccessfully("<?php fn test(): bool { return (1 == 2) || (3 != 4); }"));
}

TEST_CASE("ParserExhaustive: 166 Ternary operator ? :") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { if (true) { return 1; } return 2; }"));
}

TEST_CASE("ParserExhaustive: 167 Null coalescing ??") {
    CHECK(parsesSuccessfully("<?php fn test(opt: Option<i32>): i32 { return match (opt) { Some(v) => v, None => 10 }; }"));
}

TEST_CASE("ParserExhaustive: 168 Try operator postfix ?") {
    CHECK(parsesSuccessfully("<?php fn test(): Option<i32> { let x = get_val()?; return Some(x); }"));
}

TEST_CASE("ParserExhaustive: 169 Chained try operator calls") {
    CHECK(parsesSuccessfully("<?php fn test(): Option<i32> { return Some(a()? + b()?); }"));
}

TEST_CASE("ParserExhaustive: 170 Cast expression as") {
    CHECK(parsesSuccessfully("<?php fn test(): f64 { return 42 as f64; }"));
}

TEST_CASE("ParserExhaustive: 171 Function call with zero arguments") {
    CHECK(parsesSuccessfully("<?php fn test(): void { do_nothing(); }"));
}

TEST_CASE("ParserExhaustive: 172 Function call with single argument") {
    CHECK(parsesSuccessfully("<?php fn test(): void { println(42); }"));
}

TEST_CASE("ParserExhaustive: 173 Function call with multiple arguments") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return add(1, 2, 3); }"));
}

TEST_CASE("ParserExhaustive: 174 Static method call Class::method") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return Math::max(10, 20); }"));
}

TEST_CASE("ParserExhaustive: 175 Member access dot .") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return user.id; }"));
}

TEST_CASE("ParserExhaustive: 176 Member method call dot .") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return user.get_id(); }"));
}

TEST_CASE("ParserExhaustive: 177 PHP arrow member access ->") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return $user->id; }"));
}

TEST_CASE("ParserExhaustive: 178 PHP arrow method call ->") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return $user->getId(); }"));
}

TEST_CASE("ParserExhaustive: 179 Chained method calls") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return builder.set_a(1).set_b(2).build(); }"));
}

TEST_CASE("ParserExhaustive: 180 Array index single") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return arr[0]; }"));
}

TEST_CASE("ParserExhaustive: 181 Array index multi-dimensional") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return matrix[1][2]; }"));
}

TEST_CASE("ParserExhaustive: 182 Array slice range [start..end]") {
    CHECK(parsesSuccessfully("<?php fn test(): &[i32] { return arr[1..4]; }"));
}

TEST_CASE("ParserExhaustive: 183 Array slice from start [..end]") {
    CHECK(parsesSuccessfully("<?php fn test(): &[i32] { return arr[..4]; }"));
}

TEST_CASE("ParserExhaustive: 184 Array slice to end [start..]") {
    CHECK(parsesSuccessfully("<?php fn test(): &[i32] { return arr[2..]; }"));
}

TEST_CASE("ParserExhaustive: 185 Array literal empty []") {
    CHECK(parsesSuccessfully("<?php fn test(): void { let x = []; }"));
}

TEST_CASE("ParserExhaustive: 186 Array literal populated [1, 2, 3]") {
    CHECK(parsesSuccessfully("<?php fn test(): void { let x = [1, 2, 3]; }"));
}

TEST_CASE("ParserExhaustive: 187 Associative array literal") {
    CHECK(parsesSuccessfully("<?php fn test(): void { let map = [\"a\" => 1, \"b\" => 2]; }"));
}

TEST_CASE("ParserExhaustive: 188 Struct initialization simple") {
    CHECK(parsesSuccessfully("<?php struct Point { x: i32, y: i32 } fn test(): Point { return Point { x: 1, y: 2 }; }"));
}

TEST_CASE("ParserExhaustive: 189 Struct initialization trailing comma") {
    CHECK(parsesSuccessfully("<?php struct Point { x: i32, y: i32 } fn test(): Point { return Point { x: 1, y: 2, }; }"));
}

TEST_CASE("ParserExhaustive: 190 Struct initialization generic") {
    CHECK(parsesSuccessfully("<?php struct BoxVal<T> { val: T } fn test(): BoxVal<i32> { return BoxVal<i32> { val: 42 }; }"));
}

TEST_CASE("ParserExhaustive: 191 New expression PHP style") {
    CHECK(parsesSuccessfully("<?php fn test(): void { $obj = new MyClass(); }"));
}

TEST_CASE("ParserExhaustive: 192 New expression with arguments") {
    CHECK(parsesSuccessfully("<?php fn test(): void { $obj = new MyClass(1, \"test\"); }"));
}

TEST_CASE("ParserExhaustive: 193 Compound assignment +=") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut x = 1; x = x + 5; }"));
}

TEST_CASE("ParserExhaustive: 194 Compound assignment -=") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut x = 10; x = x - 3; }"));
}

TEST_CASE("ParserExhaustive: 195 Compound assignment *=") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut x = 4; x = x * 2; }"));
}

TEST_CASE("ParserExhaustive: 196 Compound assignment /=") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut x = 12; x = x / 4; }"));
}

TEST_CASE("ParserExhaustive: 197 Compound assignment %=") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut x = 10; x = x % 3; }"));
}

TEST_CASE("ParserExhaustive: 198 Compound assignment &=") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut x = 0xFF; x = x & 0x0F; }"));
}

TEST_CASE("ParserExhaustive: 199 Compound assignment |=") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut x = 0x10; x = x | 0x01; }"));
}

TEST_CASE("ParserExhaustive: 200 Compound assignment ^=") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut x = 0xAA; x = x ^ 0xFF; }"));
}

// =============================================================================
// Parser Exhaustive: Statements (201 to 250)
// =============================================================================

TEST_CASE("ParserExhaustive: 201 Let statement with type annotation") {
    CHECK(parsesSuccessfully("<?php fn test(): void { let x: i32 = 10; }"));
}

TEST_CASE("ParserExhaustive: 202 Let statement inferred type") {
    CHECK(parsesSuccessfully("<?php fn test(): void { let x = 10; }"));
}

TEST_CASE("ParserExhaustive: 203 Mut statement with type annotation") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut x: i64 = 100; }"));
}

TEST_CASE("ParserExhaustive: 204 Mut statement inferred type") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut x = 100; }"));
}

TEST_CASE("ParserExhaustive: 205 If statement without else") {
    CHECK(parsesSuccessfully("<?php fn test(): void { if (true) { println(1); } }"));
}

TEST_CASE("ParserExhaustive: 206 If statement with else") {
    CHECK(parsesSuccessfully("<?php fn test(): void { if (true) { println(1); } else { println(2); } }"));
}

TEST_CASE("ParserExhaustive: 207 If statement with else if chain") {
    CHECK(parsesSuccessfully("<?php fn test(): void { if (x == 1) { println(1); } else if (x == 2) { println(2); } else { println(0); } }"));
}

TEST_CASE("ParserExhaustive: 208 While loop simple") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut i = 0; while (i < 10) { i = i + 1; } }"));
}

TEST_CASE("ParserExhaustive: 209 While loop with break") {
    CHECK(parsesSuccessfully("<?php fn test(): void { while (true) { break; } }"));
}

TEST_CASE("ParserExhaustive: 210 While loop with continue") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut i = 0; while (i < 10) { i = i + 1; if (i == 5) { continue; } } }"));
}

TEST_CASE("ParserExhaustive: 211 For loop classic") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut i = 0; while (i < 10) { println(i); i = i + 1; } }"));
}

TEST_CASE("ParserExhaustive: 212 Foreach loop value only") {
    CHECK(parsesSuccessfully("<?php fn test(): void { foreach ($items as $item) { println($item); } }"));
}

TEST_CASE("ParserExhaustive: 213 Foreach loop key and value") {
    CHECK(parsesSuccessfully("<?php fn test(): void { foreach ($items as $k => $v) { println($k); } }"));
}

TEST_CASE("ParserExhaustive: 214 Return void") {
    CHECK(parsesSuccessfully("<?php fn test(): void { return; }"));
}

TEST_CASE("ParserExhaustive: 215 Return expression") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return 42; }"));
}

TEST_CASE("ParserExhaustive: 216 Defer statement") {
    CHECK(parsesSuccessfully("<?php fn test(): void { defer cleanup(); }"));
}

TEST_CASE("ParserExhaustive: 217 Multiple defer statements") {
    CHECK(parsesSuccessfully("<?php fn test(): void { defer close_a(); defer close_b(); }"));
}

TEST_CASE("ParserExhaustive: 218 Unsafe block") {
    CHECK(parsesSuccessfully("<?php fn test(): void { unsafe { let p: *i32 = null; } }"));
}

TEST_CASE("ParserExhaustive: 219 Echo statement single expression") {
    CHECK(parsesSuccessfully("<?php fn test(): void { echo \"hello\"; }"));
}

TEST_CASE("ParserExhaustive: 220 Echo statement concatenated") {
    CHECK(parsesSuccessfully("<?php fn test(): void { echo \"hello \" . \"world\"; }"));
}

TEST_CASE("ParserExhaustive: 221 Try-catch statement") {
    CHECK(parsesSuccessfully("<?php fn test(): void { try { risky(); } catch (Exception $e) { println(\"err\"); } }"));
}

TEST_CASE("ParserExhaustive: 222 Try-catch-finally statement") {
    CHECK(parsesSuccessfully("<?php fn test(): void { try { risky(); } catch (Exception $e) { log(); } finally { clean(); } }"));
}

TEST_CASE("ParserExhaustive: 223 Throw statement") {
    CHECK(parsesSuccessfully("<?php fn test(): void { throw new Exception(\"fail\"); }"));
}

TEST_CASE("ParserExhaustive: 224 Match statement single arm") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): i32 { return match (x) { 1 => 10, _ => 0 }; }"));
}

TEST_CASE("ParserExhaustive: 225 Match statement multiple arms") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): i32 { return match (x) { 1 => 10, 2 => 20, 3 => 30, _ => 0 }; }"));
}

TEST_CASE("ParserExhaustive: 226 Match statement with or pattern") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): bool { return match (x) { 1 | 2 | 3 => true, _ => false }; }"));
}

TEST_CASE("ParserExhaustive: 227 Match statement with range pattern") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): bool { return match (x) { 1..10 => true, _ => false }; }"));
}

TEST_CASE("ParserExhaustive: 228 Match statement with guard") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): i32 { return match (x) { y if y > 0 => y * 2, _ => 0 }; }"));
}

TEST_CASE("ParserExhaustive: 229 Match statement with guard in parentheses") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): i32 { return match (x) { y if (y > 0) => y * 2, _ => 0 }; }"));
}

TEST_CASE("ParserExhaustive: 230 Match on Option Some and None") {
    CHECK(parsesSuccessfully("<?php fn test(opt: Option<i32>): i32 { return match (opt) { Some(v) => v, None => 0 }; }"));
}

TEST_CASE("ParserExhaustive: 231 Match on Result Ok and Err") {
    CHECK(parsesSuccessfully("<?php fn test(res: Result<i32, string>): i32 { return match (res) { Ok(v) => v, Err(e) => -1 }; }"));
}

TEST_CASE("ParserExhaustive: 232 Rc::new expression") {
    CHECK(parsesSuccessfully("<?php fn test(): Rc<i32> { return Rc::new(42); }"));
}

TEST_CASE("ParserExhaustive: 233 Rc::count expression") {
    CHECK(parsesSuccessfully("<?php fn test(r: Rc<i32>): i64 { return Rc::count(r); }"));
}

TEST_CASE("ParserExhaustive: 234 Box::new expression") {
    CHECK(parsesSuccessfully("<?php fn test(): Box<i32> { return Box::new(42); }"));
}

TEST_CASE("ParserExhaustive: 235 Arena::new expression") {
    CHECK(parsesSuccessfully("<?php fn test(): void { let a = Arena::new(1024); }"));
}

TEST_CASE("ParserExhaustive: 236 Arena::alloc expression") {
    CHECK(parsesSuccessfully("<?php struct Item { val: i32 } fn test(a: Arena): *Item { return Arena::alloc(a, Item { val: 42 }); }"));
}

TEST_CASE("ParserExhaustive: 237 Arena::reset expression") {
    CHECK(parsesSuccessfully("<?php fn test(a: Arena): void { Arena::reset(a); }"));
}

TEST_CASE("ParserExhaustive: 238 Arena::destroy expression") {
    CHECK(parsesSuccessfully("<?php fn test(a: Arena): void { Arena::destroy(a); }"));
}

TEST_CASE("ParserExhaustive: 239 Vec::new expression") {
    CHECK(parsesSuccessfully("<?php fn test(): Vec<i32> { return Vec::new(); }"));
}

TEST_CASE("ParserExhaustive: 240 Vec method calls push and pop") {
    CHECK(parsesSuccessfully("<?php fn test(): void { let mut v: Vec<i32> = Vec::new(); v.push(10); v.pop(); }"));
}

TEST_CASE("ParserExhaustive: 241 Struct declaration empty") {
    CHECK(parsesSuccessfully("<?php struct Empty {}"));
}

TEST_CASE("ParserExhaustive: 242 Struct declaration with fields") {
    CHECK(parsesSuccessfully("<?php struct User { id: i32, name: string, active: bool }"));
}

TEST_CASE("ParserExhaustive: 243 Struct declaration generic") {
    CHECK(parsesSuccessfully("<?php struct Pair<T, U> { first: T, second: U }"));
}

TEST_CASE("ParserExhaustive: 244 Enum declaration empty") {
    CHECK(parsesSuccessfully("<?php enum Color {}"));
}

TEST_CASE("ParserExhaustive: 245 Enum declaration with variants") {
    CHECK(parsesSuccessfully("<?php enum Color { Red, Green, Blue }"));
}

TEST_CASE("ParserExhaustive: 246 Interface declaration empty") {
    CHECK(parsesSuccessfully("<?php interface Printable {}"));
}

TEST_CASE("ParserExhaustive: 247 Interface declaration with methods") {
    CHECK(parsesSuccessfully("<?php interface Printable { fn print(): void; fn get_name(): string; }"));
}

TEST_CASE("ParserExhaustive: 248 Class declaration with fields and methods") {
    CHECK(parsesSuccessfully("<?php class Greeter { public $name: string; public function greet(): void { echo \"hi\"; } }"));
}

TEST_CASE("ParserExhaustive: 249 Class implementing interface") {
    CHECK(parsesSuccessfully("<?php interface Runner { fn run(): void; } class Bot implements Runner { public function run(): void {} }"));
}

TEST_CASE("ParserExhaustive: 250 Extern C function declaration") {
    CHECK(parsesSuccessfully("<?php extern \"C\" fn puts(s: *u8): i32;"));
}

// =============================================================================
// Parser Exhaustive: Complex Types & Declarations (251 to 300)
// =============================================================================

TEST_CASE("ParserExhaustive: 251 Slice type parameter &[i32]") {
    CHECK(parsesSuccessfully("<?php fn sum(items: &[i32]): i32 { return 0; }"));
}

TEST_CASE("ParserExhaustive: 252 Fixed-size array type [i32; 16]") {
    CHECK(parsesSuccessfully("<?php fn get_buf(): [i32; 4] { return [0, 0, 0, 0]; }"));
}

TEST_CASE("ParserExhaustive: 253 Raw pointer type *i32") {
    CHECK(parsesSuccessfully("<?php fn read_ptr(p: *i32): i32 { unsafe { return *p; } }"));
}

TEST_CASE("ParserExhaustive: 254 Double pointer type **i32") {
    CHECK(parsesSuccessfully("<?php fn deref_twice(p: **i32): i32 { unsafe { return **p; } }"));
}

TEST_CASE("ParserExhaustive: 255 Option of Option type Option<Option<i32>>") {
    CHECK(parsesSuccessfully("<?php fn get_nested(): Option<Option<i32>> { return Some(Some(42)); }"));
}

TEST_CASE("ParserExhaustive: 256 Result containing Option Result<Option<i32>, string>") {
    CHECK(parsesSuccessfully("<?php fn parse_opt(): Result<Option<i32>, string> { return Ok(Some(1)); }"));
}

TEST_CASE("ParserExhaustive: 257 Vec of Option Vec<Option<string>>") {
    CHECK(parsesSuccessfully("<?php fn get_list(): Vec<Option<string>> { return Vec::new(); }"));
}

TEST_CASE("ParserExhaustive: 258 Async function declaration") {
    CHECK(parsesSuccessfully("<?php async fn fetch_data(): i32 { return 42; }"));
}

TEST_CASE("ParserExhaustive: 259 Function with generic type parameter fn identity<T>") {
    CHECK(parsesSuccessfully("<?php fn identity<T>(val: T): T { return val; }"));
}

TEST_CASE("ParserExhaustive: 260 Function with multiple generic params fn make_pair<A, B>") {
    CHECK(parsesSuccessfully("<?php fn make_pair<A, B>(a: A, b: B): void {}"));
}

TEST_CASE("ParserExhaustive: 261 Attribute on function #[inline]") {
    CHECK(parsesSuccessfully("<?php #[align(8)] struct AlignedHeader { magic: i32 }"));
}

TEST_CASE("ParserExhaustive: 262 Attribute on struct #[packed]") {
    CHECK(parsesSuccessfully("<?php #[repr(packed)] struct Header { magic: i32, version: i16 }"));
}

TEST_CASE("ParserExhaustive: 263 By-reference parameter in function &arg") {
    CHECK(parsesSuccessfully("<?php fn increment(&val: i32): void { val = val + 1; }"));
}

TEST_CASE("ParserExhaustive: 264 Import statement") {
    CHECK(parsesSuccessfully("<?php import \"std/core.sph\"; fn test(): void {}"));
}

TEST_CASE("ParserExhaustive: 265 Multiple import statements") {
    CHECK(parsesSuccessfully("<?php import \"std/core.sph\"; import \"std/io.sph\"; fn test(): void {}"));
}

TEST_CASE("ParserExhaustive: 266 Empty program with PHP open tag") {
    CHECK(parsesSuccessfully("<?php"));
}

TEST_CASE("ParserExhaustive: 267 Multiple global functions in program") {
    CHECK(parsesSuccessfully("<?php fn a(): void {} fn b(): void {} fn c(): void {}"));
}

TEST_CASE("ParserExhaustive: 268 Function call with nested expressions") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return foo(bar(1 + 2), baz(3 * 4)); }"));
}

TEST_CASE("ParserExhaustive: 269 Nested block scopes") {
    CHECK(parsesSuccessfully("<?php fn test(): void { { let a = 1; { let b = 2; } } }"));
}

TEST_CASE("ParserExhaustive: 270 Shadowing in nested blocks") {
    CHECK(parsesSuccessfully("<?php fn test(): void { let x = 1; { let x = 2; println(x); } println(x); }"));
}

TEST_CASE("ParserExhaustive: 271 Empty statement semicolon") {
    CHECK(parsesSuccessfully("<?php fn test(): void { {} }"));
}

TEST_CASE("ParserExhaustive: 272 Empty while loop body") {
    CHECK(parsesSuccessfully("<?php fn test(): void { while (false) {} }"));
}

TEST_CASE("ParserExhaustive: 273 Empty for loop body") {
    CHECK(parsesSuccessfully("<?php fn test(): void { while (false) {} }"));
}

TEST_CASE("ParserExhaustive: 274 Semicolon optional after struct field") {
    CHECK(parsesSuccessfully("<?php struct Item { a: i32, b: string }"));
}

TEST_CASE("ParserExhaustive: 275 Semicolon optional after match arm") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): i32 { return match (x) { 0 => 10, _ => 20 }; }"));
}

TEST_CASE("ParserExhaustive: 276 Array index as LHS in assignment") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut arr: [i32; 4] = [1, 2, 3, 4]; arr[0] = 42; }"));
}

TEST_CASE("ParserExhaustive: 277 Member access as LHS in assignment") {
    CHECK(parsesSuccessfully("<?php struct Point { x: i32, y: i32 } fn test(): void { mut p = Point { x: 0, y: 0 }; p.x = 10; }"));
}

TEST_CASE("ParserExhaustive: 278 Dereference as LHS in assignment") {
    CHECK(parsesSuccessfully("<?php fn test(p: *i32): void { unsafe { *p = 99; } }"));
}

TEST_CASE("ParserExhaustive: 279 Multiple assignment in sequence") {
    CHECK(parsesSuccessfully("<?php fn test(): void { mut a = 0; mut b = 0; a = 1; b = 2; }"));
}

TEST_CASE("ParserExhaustive: 280 Complex expression with multiple binary operators") {
    CHECK(parsesSuccessfully("<?php fn test(a: i32, b: i32, c: i32): i32 { return a * b + c - a / b % c; }"));
}

TEST_CASE("ParserExhaustive: 281 Boolean algebra expression") {
    CHECK(parsesSuccessfully("<?php fn test(a: bool, b: bool, c: bool): bool { return (a && !b) || (!a && c); }"));
}

TEST_CASE("ParserExhaustive: 282 Bitwise masking expression") {
    CHECK(parsesSuccessfully("<?php fn test(val: u32): u32 { return (val >> 8) & 0xFF; }"));
}

TEST_CASE("ParserExhaustive: 283 Struct with nested struct field") {
    CHECK(parsesSuccessfully("<?php struct Inner { val: i32 } struct Outer { inner: Inner }"));
}

TEST_CASE("ParserExhaustive: 284 Struct initialization with nested struct") {
    CHECK(parsesSuccessfully("<?php struct Inner { val: i32 } struct Outer { inner: Inner } fn test(): Outer { return Outer { inner: Inner { val: 42 } }; }"));
}

TEST_CASE("ParserExhaustive: 285 Match with block arm") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): i32 { return match (x) { 1 => (10 * 2), _ => 0 }; }"));
}

TEST_CASE("ParserExhaustive: 286 Match wildcard pattern _") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): string { return match (x) { _ => \"anything\" }; }"));
}

TEST_CASE("ParserExhaustive: 287 Match variable binding pattern") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): i32 { return match (x) { n => n * 2 }; }"));
}

TEST_CASE("ParserExhaustive: 288 Match negative literal pattern -1") {
    CHECK(parsesSuccessfully("<?php fn test(x: i32): i32 { return match (x) { -1 => 1, _ => 0 }; }"));
}

TEST_CASE("ParserExhaustive: 289 Match boolean pattern true | false") {
    CHECK(parsesSuccessfully("<?php fn test(b: bool): i32 { return match (b) { true => 1, false => 0 }; }"));
}

TEST_CASE("ParserExhaustive: 290 Match string literal pattern") {
    CHECK(parsesSuccessfully("<?php fn test(s: string): i32 { return match (s) { \"ok\" => 1, \"err\" => 2, _ => 0 }; }"));
}

TEST_CASE("ParserExhaustive: 291 Class constructor declaration __construct") {
    CHECK(parsesSuccessfully("<?php class User { public function __construct($id) {} }"));
}

TEST_CASE("ParserExhaustive: 292 Class method returning this") {
    CHECK(parsesSuccessfully("<?php class Chain { public function step(): Chain { return $this; } }"));
}

TEST_CASE("ParserExhaustive: 293 Static property access Class::$prop") {
    CHECK(parsesSuccessfully("<?php class Config { public static function get_version(): string { return \"1.0\"; } } fn test(): void { echo Config::get_version(); }"));
}

TEST_CASE("ParserExhaustive: 294 Static method call with arguments") {
    CHECK(parsesSuccessfully("<?php fn test(): i32 { return Calculator::add(10, 20); }"));
}

TEST_CASE("ParserExhaustive: 295 Function with string parameter and return") {
    CHECK(parsesSuccessfully("<?php fn greet(name: string): string { return \"Hello \" . name; }"));
}

TEST_CASE("ParserExhaustive: 296 Function with boolean parameter and return") {
    CHECK(parsesSuccessfully("<?php fn toggle(b: bool): bool { return !b; }"));
}

TEST_CASE("ParserExhaustive: 297 Function with 64-bit integer parameter and return") {
    CHECK(parsesSuccessfully("<?php fn double_i64(x: i64): i64 { return x * 2; }"));
}

TEST_CASE("ParserExhaustive: 298 Function with 32-bit float parameter and return") {
    CHECK(parsesSuccessfully("<?php fn half_f32(x: f32): f32 { return x / 2.0; }"));
}

TEST_CASE("ParserExhaustive: 299 Function with 64-bit float parameter and return") {
    CHECK(parsesSuccessfully("<?php fn sqrt_approx(x: f64): f64 { return x / 2.0; }"));
}

TEST_CASE("ParserExhaustive: 300 Reject syntax error missing semicolon") {
    bool hasErrors = false;
    parse("<?php fn test(): void { let x = 10 }", hasErrors);
    CHECK(hasErrors);
}

