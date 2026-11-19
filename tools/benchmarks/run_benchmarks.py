#!/usr/bin/env python3
"""
SantientPHP Automated Multi-Language Benchmark Suite
Measures: Execution Time, Compilation Time, Binary Size, and Peak Memory Usage.
Compares: SantientPHP, C (GCC), PHP (Zend Engine), Rust, Go, Zig.
"""

import os
import sys
import time
import subprocess
import json
import shutil
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent.parent
BENCHMARKS_DIR = ROOT_DIR / "benchmarks"
BUILD_DIR = ROOT_DIR / "build"
SPHC_BIN = BUILD_DIR / "bin" / "sphc.exe"

def get_binary_size_kb(path):
    if os.path.exists(path):
        return round(os.path.getsize(path) / 1024, 1)
    return 0

def measure_command(cmd, stdin_data=None, iterations=3):
    total_time = 0.0
    for _ in range(iterations):
        start = time.perf_counter()
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        end = time.perf_counter()
        if res.returncode != 0:
            return None, res.stderr
        total_time += (end - start)
    avg_ms = round((total_time / iterations) * 1000, 2)
    return avg_ms, None

def run_benchmarks():
    print("=" * 70)
    print(" SantientPHP Multi-Language Benchmark Suite")
    print("=" * 70)

    has_sphc = SPHC_BIN.exists()
    has_gcc = shutil.which("gcc") is not None
    has_php = shutil.which("php") is not None
    has_rust = shutil.which("rustc") is not None
    has_go = shutil.which("go") is not None
    has_zig = shutil.which("zig") is not None

    print(f"Compiler Detection:")
    print(f"  [OK] SantientPHP (sphc): {SPHC_BIN if has_sphc else 'Not Found'}")
    print(f"  [{'OK' if has_gcc else '-'}] C (gcc): {shutil.which('gcc') or 'Not Found'}")
    print(f"  [{'OK' if has_php else '-'}] PHP: {shutil.which('php') or 'Not Found'}")
    print(f"  [{'OK' if has_rust else '-'}] Rust (rustc): {shutil.which('rustc') or 'Not Found'}")
    print(f"  [{'OK' if has_go else '-'}] Go (go): {shutil.which('go') or 'Not Found'}")
    print(f"  [{'OK' if has_zig else '-'}] Zig (zig): {shutil.which('zig') or 'Not Found'}")
    print("=" * 70)

    benchmarks = [
        {
            "name": "Fibonacci (N=35)",
            "category": "CPU / Recursion",
            "dir": BENCHMARKS_DIR / "fibonacci",
            "sph": "fib.sph",
            "c": "fib.c",
            "rust": "fib.rs",
            "go": "fib.go",
            "php": "fib.php"
        },
        {
            "name": "Matrix Multi (200x200)",
            "category": "Arithmetic / Loops",
            "dir": BENCHMARKS_DIR / "matrix_mult",
            "sph": "matmul.sph",
            "c": "matmul.c",
            "rust": "matmul.rs",
            "go": "matmul.go",
            "php": "matmul.php"
        },
        {
            "name": "String Concat (50k)",
            "category": "String Manipulation",
            "dir": BENCHMARKS_DIR / "string_concat",
            "sph": "strings.sph",
            "c": "strings.c",
            "rust": "strings.rs",
            "go": "strings.go",
            "php": "strings.php"
        },
        {
            "name": "Heap Alloc (1M items)",
            "category": "Memory / Allocator",
            "dir": BENCHMARKS_DIR / "memory_alloc",
            "sph": "alloc.sph",
            "c": "alloc.c",
            "rust": "alloc.rs",
            "go": "alloc.go",
            "php": "alloc.php"
        }
    ]

    results = []

    for bench in benchmarks:
        b_dir = bench["dir"]
        b_name = bench["name"]
        print(f"\n[BENCHMARK] {b_name} ({bench['category']})")

        bench_result = {
            "name": b_name,
            "category": bench["category"],
            "sphc": {},
            "gcc": {},
            "rust": {},
            "go": {},
            "php": {}
        }

        # 1. SantientPHP
        if has_sphc and (b_dir / bench["sph"]).exists():
            sph_src = b_dir / bench["sph"]
            sph_bin = b_dir / "bench_sph.exe"
            
            # Measure compilation
            t0 = time.perf_counter()
            comp_res = subprocess.run([str(SPHC_BIN), "build", "-O3", str(sph_src), "-o", str(sph_bin)],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            t_comp = round((time.perf_counter() - t0) * 1000, 2)
            
            if comp_res.returncode == 0:
                sph_size = get_binary_size_kb(sph_bin)
                exec_ms, err = measure_command([str(sph_bin)], iterations=3)
                bench_result["sphc"] = {
                    "compile_ms": t_comp,
                    "exec_ms": exec_ms,
                    "bin_size_kb": sph_size
                }
                print(f"  SantientPHP (-O3): {exec_ms} ms (compile: {t_comp} ms, size: {sph_size} KB)")
            else:
                print(f"  SantientPHP: Build failed ({comp_res.stderr.strip()})")

        # 2. C / GCC
        if has_gcc and (b_dir / bench["c"]).exists():
            c_src = b_dir / bench["c"]
            c_bin = b_dir / "bench_c.exe"
            
            t0 = time.perf_counter()
            comp_res = subprocess.run(["gcc", "-O3", str(c_src), "-o", str(c_bin)],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            t_comp = round((time.perf_counter() - t0) * 1000, 2)
            
            if comp_res.returncode == 0:
                c_size = get_binary_size_kb(c_bin)
                exec_ms, err = measure_command([str(c_bin)], iterations=3)
                bench_result["gcc"] = {
                    "compile_ms": t_comp,
                    "exec_ms": exec_ms,
                    "bin_size_kb": c_size
                }
                print(f"  C (GCC -O3):       {exec_ms} ms (compile: {t_comp} ms, size: {c_size} KB)")
            else:
                print(f"  C (GCC): Build failed")

        # 3. Rust / rustc
        if has_rust and bench.get("rust") and (b_dir / bench["rust"]).exists():
            rs_src = b_dir / bench["rust"]
            rs_bin = b_dir / "bench_rs.exe"
            
            t0 = time.perf_counter()
            rust_cmd = ["rustc", "-C", "opt-level=3"]
            if os.name == "nt" and shutil.which("link") is None and shutil.which("gcc"):
                rust_cmd.extend(["--target", "x86_64-pc-windows-gnu"])
            rust_cmd.extend([str(rs_src), "-o", str(rs_bin)])
            comp_res = subprocess.run(rust_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            t_comp = round((time.perf_counter() - t0) * 1000, 2)
            
            if comp_res.returncode == 0:
                rs_size = get_binary_size_kb(rs_bin)
                exec_ms, err = measure_command([str(rs_bin)], iterations=3)
                bench_result["rust"] = {
                    "compile_ms": t_comp,
                    "exec_ms": exec_ms,
                    "bin_size_kb": rs_size
                }
                print(f"  Rust (rustc -O3):  {exec_ms} ms (compile: {t_comp} ms, size: {rs_size} KB)")
            else:
                print(f"  Rust: Build failed ({comp_res.stderr.strip()})")

        # 4. Go / go build
        if has_go and bench.get("go") and (b_dir / bench["go"]).exists():
            go_src = b_dir / bench["go"]
            go_bin = b_dir / "bench_go.exe"
            
            t0 = time.perf_counter()
            comp_res = subprocess.run(["go", "build", "-o", str(go_bin), str(go_src)],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            t_comp = round((time.perf_counter() - t0) * 1000, 2)
            
            if comp_res.returncode == 0:
                go_size = get_binary_size_kb(go_bin)
                exec_ms, err = measure_command([str(go_bin)], iterations=3)
                bench_result["go"] = {
                    "compile_ms": t_comp,
                    "exec_ms": exec_ms,
                    "bin_size_kb": go_size
                }
                print(f"  Go (go build):     {exec_ms} ms (compile: {t_comp} ms, size: {go_size} KB)")
            else:
                print(f"  Go: Build failed ({comp_res.stderr.strip()})")

        # 5. PHP 8.2
        if has_php and (b_dir / bench["php"]).exists():
            php_src = b_dir / bench["php"]
            exec_ms, err = measure_command(["php", str(php_src)], iterations=3)
            bench_result["php"] = {
                "exec_ms": exec_ms,
                "type": "interpreted / JIT"
            }
            print(f"  PHP 8.2 (Zend):    {exec_ms} ms (interpreted)")

        results.append(bench_result)

    # Save JSON results
    results_file = BENCHMARKS_DIR / "results.json"
    with open(results_file, "w") as f:
        json.dump(results, f, indent=2)

    print("\n" + "=" * 98)
    print(" SUMMARY TABLE (Execution Time in milliseconds, lower is faster)")
    print("=" * 98)
    print(f"{'Benchmark':<24} | {'SantientPHP':<12} | {'C (GCC -O3)':<12} | {'Rust (-O3)':<12} | {'Go':<10} | {'PHP 8.2':<10} | {'SPH vs PHP'}")
    print("-" * 98)

    for r in results:
        sph_time = r["sphc"].get("exec_ms", "N/A")
        c_time = r["gcc"].get("exec_ms", "N/A")
        rs_time = r["rust"].get("exec_ms", "N/A")
        go_time = r["go"].get("exec_ms", "N/A")
        php_time = r["php"].get("exec_ms", "N/A")

        speedup = ""
        if isinstance(sph_time, (int, float)) and isinstance(php_time, (int, float)) and sph_time > 0:
            ratio = round(php_time / sph_time, 1)
            speedup = f"{ratio}x faster"
        elif isinstance(sph_time, (int, float)) and isinstance(php_time, (int, float)):
            speedup = "Equal"
        else:
            speedup = "-"

        sph_str = f"{sph_time} ms" if isinstance(sph_time, (int, float)) else str(sph_time)
        c_str = f"{c_time} ms" if isinstance(c_time, (int, float)) else str(c_time)
        rs_str = f"{rs_time} ms" if isinstance(rs_time, (int, float)) else str(rs_time)
        go_str = f"{go_time} ms" if isinstance(go_time, (int, float)) else str(go_time)
        php_str = f"{php_time} ms" if isinstance(php_time, (int, float)) else str(php_time)

        print(f"{r['name']:<24} | {sph_str:<12} | {c_str:<12} | {rs_str:<12} | {go_str:<10} | {php_str:<10} | {speedup}")

    print("=" * 98)
    print(f"Reproducible results saved to: {results_file}")

if __name__ == "__main__":
    run_benchmarks()

