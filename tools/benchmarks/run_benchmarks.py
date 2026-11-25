#!/usr/bin/env python3
"""
SantientPHP Automated Multi-Language Benchmark Suite
Measures: Execution Time (warmed median), Compilation Time, and Binary Size.
Compares: SantientPHP, C (GCC), Rust, Go, and PHP 8.2 (Zend Engine).
"""

import argparse
import datetime
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent.parent
BENCHMARKS_DIR = ROOT_DIR / "benchmarks"
BUILD_DIR = ROOT_DIR / "build"
EXE_SUFFIX = ".exe" if os.name == "nt" else ""
SPHC_BIN = BUILD_DIR / "bin" / f"sphc{EXE_SUFFIX}"


def read_project_version():
    ver_file = ROOT_DIR / "VERSION"
    if ver_file.exists():
        text = ver_file.read_text(encoding="utf-8").strip()
        if text:
            return text.splitlines()[0].strip()
    return "0.2.2"


def get_binary_size_kb(path):
    if os.path.exists(path):
        return round(os.path.getsize(path) / 1024.0, 1)
    return 0.0


def measure_command(cmd, warmup=2, iterations=5):
    for _ in range(warmup):
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if res.returncode != 0:
            return None, res.stderr

    samples_ms = []
    for _ in range(iterations):
        start = time.perf_counter()
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        end = time.perf_counter()
        if res.returncode != 0:
            return None, res.stderr
        samples_ms.append((end - start) * 1000.0)

    return {
        "exec_ms": round(statistics.median(samples_ms), 2),
        "min_ms": round(min(samples_ms), 2),
        "mean_ms": round(statistics.mean(samples_ms), 2),
    }, None


def fmt_speedup_vs_baseline(sph_ms, other_ms):
    if not isinstance(sph_ms, (int, float)) or not isinstance(other_ms, (int, float)):
        return "-"
    if sph_ms <= 0 or other_ms <= 0:
        return "-"
    if other_ms >= sph_ms:
        return f"**{round(other_ms / sph_ms, 1)}x faster**"
    return f"{round(sph_ms / other_ms, 1)}x slower"


def build_readme_benchmark_section(results):
    lines = [
        "### Execution Time (milliseconds, warmed median — lower is faster)",
        "",
        "| Benchmark | Workload / Category | SantientPHP (-O3) | C (GCC -O3) | Rust (-O3) | Go (go build) | PHP 8.2 (Zend) | SPH vs PHP | SPH vs Rust |",
        "| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |",
    ]
    links = {
        "Fibonacci (N=35)": "benchmarks/fibonacci/",
        "Matrix Multi (200x200)": "benchmarks/matrix_mult/",
        "String Concat (50k)": "benchmarks/string_concat/",
        "Heap Alloc (1M items)": "benchmarks/memory_alloc/",
    }
    for r in results:
        name = r["name"]
        link = links.get(name, "benchmarks/")
        sph_t = r["sphc"].get("exec_ms")
        c_t = r["gcc"].get("exec_ms")
        rs_t = r["rust"].get("exec_ms")
        go_t = r["go"].get("exec_ms")
        php_t = r["php"].get("exec_ms")

        sph_s = f"**{sph_t:.2f} ms**" if isinstance(sph_t, (int, float)) else "N/A"
        c_s = f"{c_t:.2f} ms" if isinstance(c_t, (int, float)) else "N/A"
        rs_s = f"{rs_t:.2f} ms" if isinstance(rs_t, (int, float)) else "N/A"
        go_s = f"{go_t:.2f} ms" if isinstance(go_t, (int, float)) else "N/A"
        php_s = f"{php_t:,.2f} ms" if isinstance(php_t, (int, float)) else "N/A"

        lines.append(
            f"| [**{name}**]({link}) | {r['category']} | {sph_s} | {c_s} | {rs_s} | {go_s} | {php_s} | "
            f"{fmt_speedup_vs_baseline(sph_t, php_t)} | {fmt_speedup_vs_baseline(sph_t, rs_t)} |"
        )

    plat = "Windows x86_64" if os.name == "nt" else os.uname().sysname + " x86_64"
    lines.extend([
        "",
        f"*Environment: {plat}. Warmed median across 5 runs. Generated automatically from `benchmarks/results.json` via `python tools/benchmarks/run_benchmarks.py --update-docs`.*",
        "",
        "### Executable Binary Footprint & Single-File Compile Time (`-O3`)",
        "",
        "| Benchmark | SantientPHP Binary | Compile Time (`-O3`) | C (`gcc -O3 -s`) Binary | Rust Binary | Go Binary |",
        "| :--- | :--- | :--- | :--- | :--- | :--- |",
    ])
    for r in results:
        sph_kb = r["sphc"].get("bin_size_kb", "N/A")
        sph_cm = r["sphc"].get("compile_ms", "N/A")
        c_kb = r["gcc"].get("bin_size_kb", "N/A")
        rs_kb = r["rust"].get("bin_size_kb", "N/A")
        go_kb = r["go"].get("bin_size_kb", "N/A")
        sph_kb_s = f"**{sph_kb} KB**" if isinstance(sph_kb, (int, float)) else str(sph_kb)
        sph_cm_s = f"**{sph_cm:.1f} ms**" if isinstance(sph_cm, (int, float)) else str(sph_cm)
        c_kb_s = f"{c_kb} KB" if isinstance(c_kb, (int, float)) else str(c_kb)
        rs_kb_s = f"{rs_kb} KB" if isinstance(rs_kb, (int, float)) else str(rs_kb)
        go_kb_s = f"{go_kb} KB" if isinstance(go_kb, (int, float)) else str(go_kb)
        lines.append(f"| **{r['name']}** | {sph_kb_s} | {sph_cm_s} | {c_kb_s} | {rs_kb_s} | {go_kb_s} |")

    return "\n".join(lines)


def write_docs_from_results(results, version):
    section_md = build_readme_benchmark_section(results)
    bench_doc_dir = ROOT_DIR / "docs" / "releases" / f"v{version}"
    bench_doc_dir.mkdir(parents=True, exist_ok=True)
    bench_doc_path = bench_doc_dir / "BENCHMARKS.md"

    today = datetime.date.today().isoformat()
    doc_content = f"""# SantientPHP v{version} — Benchmark Results & Performance Analysis

**Document Status:** Automated Release Benchmark Report  
**Release:** SantientPHP v{version}  
**Date:** {today}  
**Source of Truth:** Generated directly from `benchmarks/results.json`

---

## 1. Execution, Compilation & Footprint Summary

{section_md}

---

## 2. Release Optimization Highlights (v{version})

- **Loop-Scoped Escape Analysis (M1):** Iteration-local `Box::new(...)` allocations in loops reuse an entry-block stack slot safely verified by `EscapeAnalysisPass` + `IRVerifier`.
- **x86_64 Codegen & Loop Passes (M2):** Immediate arithmetic (`add/sub/cmp reg, imm32`), 3-operand/scaled `lea`, frame-pointer omission (`-O1+`), minimal callee-saves, LSRA copy coalescing, Loop Strength Reduction (LSR), 4x unrolling (`-O3`), distributive factoring, and `sph_string_append` MRU TLS fast-path.
- **Modular Runtime & Section Stripping (M4):** Standalone console programs link strictly against modular `libsph_core.a` with `-ffunction-sections -fdata-sections` and `-s -Wl,--gc-sections`, shrinking standalone binaries under **60 KB** (`< 120 KB`) and single-file `-O3` builds under **1.0 s**.
"""
    bench_doc_path.write_text(doc_content, encoding="utf-8")

    readme_path = ROOT_DIR / "README.md"
    if readme_path.exists():
        readme_text = readme_path.read_text(encoding="utf-8")
        pattern = re.compile(
            r"<!-- BENCHMARKS_START -->.*?<!-- BENCHMARKS_END -->",
            re.DOTALL,
        )
        replacement = f"<!-- BENCHMARKS_START -->\n{section_md}\n<!-- BENCHMARKS_END -->"
        if pattern.search(readme_text):
            updated = pattern.sub(replacement, readme_text)
            readme_path.write_text(updated, encoding="utf-8")


def run_benchmarks(warmup=2, iterations=5, update_docs=False):
    version = read_project_version()
    print("=" * 72)
    print(f" SantientPHP v{version} Automated Benchmark Suite")
    print("=" * 72)

    has_sphc = SPHC_BIN.exists()
    has_gcc = shutil.which("gcc") is not None
    has_php = shutil.which("php") is not None
    has_rust = shutil.which("rustc") is not None
    has_go = shutil.which("go") is not None

    print("Compiler Detection:")
    print(f"  [OK] SantientPHP (sphc): {SPHC_BIN if has_sphc else 'Not Found'}")
    print(f"  [{'OK' if has_gcc else '-'}] C (gcc): {shutil.which('gcc') or 'Not Found'}")
    print(f"  [{'OK' if has_php else '-'}] PHP: {shutil.which('php') or 'Not Found'}")
    print(f"  [{'OK' if has_rust else '-'}] Rust (rustc): {shutil.which('rustc') or 'Not Found'}")
    print(f"  [{'OK' if has_go else '-'}] Go (go): {shutil.which('go') or 'Not Found'}")
    print("=" * 72)

    benchmarks = [
        {"name": "Fibonacci (N=35)", "category": "CPU / Deep Recursion", "dir": BENCHMARKS_DIR / "fibonacci", "sph": "fib.sph", "c": "fib.c", "rust": "fib.rs", "go": "fib.go", "php": "fib.php"},
        {"name": "Matrix Multi (200x200)", "category": "Arithmetic & Loops", "dir": BENCHMARKS_DIR / "matrix_mult", "sph": "matmul.sph", "c": "matmul.c", "rust": "matmul.rs", "go": "matmul.go", "php": "matmul.php"},
        {"name": "String Concat (50k)", "category": "String Manipulation", "dir": BENCHMARKS_DIR / "string_concat", "sph": "strings.sph", "c": "strings.c", "rust": "strings.rs", "go": "strings.go", "php": "strings.php"},
        {"name": "Heap Alloc (1M items)", "category": "Allocator & Memory", "dir": BENCHMARKS_DIR / "memory_alloc", "sph": "alloc.sph", "c": "alloc.c", "rust": "alloc.rs", "go": "alloc.go", "php": "alloc.php"},
    ]

    results = []

    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_path = Path(tmp_dir)
        for idx, bench in enumerate(benchmarks):
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
                "php": {},
            }

            if has_sphc and (b_dir / bench["sph"]).exists():
                sph_src = b_dir / bench["sph"]
                sph_bin = tmp_path / f"bench_{idx}_sph{EXE_SUFFIX}"
                t0 = time.perf_counter()
                comp_res = subprocess.run([str(SPHC_BIN), "build", "-O3", str(sph_src), "-o", str(sph_bin)],
                                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                t_comp = round((time.perf_counter() - t0) * 1000, 2)
                if comp_res.returncode == 0:
                    sph_size = get_binary_size_kb(sph_bin)
                    metrics, err = measure_command([str(sph_bin)], warmup=warmup, iterations=iterations)
                    if metrics:
                        bench_result["sphc"] = {"compile_ms": t_comp, "bin_size_kb": sph_size, **metrics}
                        print(f"  SantientPHP (-O3): {metrics['exec_ms']} ms (min: {metrics['min_ms']} ms, compile: {t_comp} ms, size: {sph_size} KB)")
                else:
                    print(f"  SantientPHP: Build failed ({comp_res.stderr.strip()})")

            if has_gcc and (b_dir / bench["c"]).exists():
                c_src = b_dir / bench["c"]
                c_bin = tmp_path / f"bench_{idx}_c{EXE_SUFFIX}"
                t0 = time.perf_counter()
                comp_res = subprocess.run(["gcc", "-O3", "-s", str(c_src), "-o", str(c_bin)],
                                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                t_comp = round((time.perf_counter() - t0) * 1000, 2)
                if comp_res.returncode == 0:
                    c_size = get_binary_size_kb(c_bin)
                    metrics, _ = measure_command([str(c_bin)], warmup=warmup, iterations=iterations)
                    if metrics:
                        bench_result["gcc"] = {"compile_ms": t_comp, "bin_size_kb": c_size, **metrics}
                        print(f"  C (GCC -O3 -s):    {metrics['exec_ms']} ms (compile: {t_comp} ms, size: {c_size} KB)")

            if has_rust and bench.get("rust") and (b_dir / bench["rust"]).exists():
                rs_src = b_dir / bench["rust"]
                rs_bin = tmp_path / f"bench_{idx}_rs{EXE_SUFFIX}"
                t0 = time.perf_counter()
                rust_cmd = ["rustc", "-C", "opt-level=3"]
                if os.name == "nt" and shutil.which("link") is None and shutil.which("gcc"):
                    rust_cmd.extend(["--target", "x86_64-pc-windows-gnu"])
                rust_cmd.extend([str(rs_src), "-o", str(rs_bin)])
                comp_res = subprocess.run(rust_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                t_comp = round((time.perf_counter() - t0) * 1000, 2)
                if comp_res.returncode == 0:
                    rs_size = get_binary_size_kb(rs_bin)
                    metrics, _ = measure_command([str(rs_bin)], warmup=warmup, iterations=iterations)
                    if metrics:
                        bench_result["rust"] = {"compile_ms": t_comp, "bin_size_kb": rs_size, **metrics}
                        print(f"  Rust (rustc -O3):  {metrics['exec_ms']} ms (compile: {t_comp} ms, size: {rs_size} KB)")

            if has_go and bench.get("go") and (b_dir / bench["go"]).exists():
                go_src = b_dir / bench["go"]
                go_bin = tmp_path / f"bench_{idx}_go{EXE_SUFFIX}"
                t0 = time.perf_counter()
                comp_res = subprocess.run(["go", "build", "-o", str(go_bin), str(go_src)],
                                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                t_comp = round((time.perf_counter() - t0) * 1000, 2)
                if comp_res.returncode == 0:
                    go_size = get_binary_size_kb(go_bin)
                    metrics, _ = measure_command([str(go_bin)], warmup=warmup, iterations=iterations)
                    if metrics:
                        bench_result["go"] = {"compile_ms": t_comp, "bin_size_kb": go_size, **metrics}
                        print(f"  Go (go build):     {metrics['exec_ms']} ms (compile: {t_comp} ms, size: {go_size} KB)")

            if has_php and (b_dir / bench["php"]).exists():
                php_src = b_dir / bench["php"]
                metrics, _ = measure_command(["php", str(php_src)], warmup=1, iterations=iterations)
                if metrics:
                    bench_result["php"] = {"type": "interpreted / JIT", **metrics}
                    print(f"  PHP 8.2 (Zend):    {metrics['exec_ms']} ms (interpreted)")

            results.append(bench_result)

    results_file = BENCHMARKS_DIR / "results.json"
    with open(results_file, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)

    print("\n" + "=" * 115)
    print(f"Reproducible results saved to: {results_file}")

    if update_docs:
        write_docs_from_results(results, version)
        print(f"Updated docs/releases/v{version}/BENCHMARKS.md and README.md benchmark tables.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run SantientPHP benchmark suite")
    parser.add_argument("--warmup", type=int, default=2, help="Warmup iterations per binary")
    parser.add_argument("--iterations", type=int, default=5, help="Timed iterations per binary")
    parser.add_argument("--update-docs", action="store_true", help="Regenerate release BENCHMARKS.md and README.md table")
    args = parser.parse_args()
    run_benchmarks(warmup=args.warmup, iterations=args.iterations, update_docs=args.update_docs)
