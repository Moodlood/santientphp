# SantientPHP Platform Compatibility & Support Matrix

## 1. Platform Support Tiers

| Tier | OS / Target Platform | Architecture | ABI Standard | Default Toolchain | Status |
|---|---|---|---|---|---|
| **Tier 1 (Primary)** | Windows 10, 11, Server 2019+ | x86_64 (AMD64) | Microsoft x64 Calling Convention | MinGW-w64 (GCC 12+ / UCRT64) & MSVC 2022 | **Fully Verified & CI-Gated** |
| **Tier 1 (Primary)** | Ubuntu 22.04+, Debian 12+, RHEL 9+ | x86_64 (AMD64) | System V AMD64 ABI | GCC 12+, Clang 15+ (glibc 2.31+) | **Fully Verified & CI-Gated** |
| **Tier 2 (Secondary)** | macOS 12+ (Monterey, Sonoma, Sequoia) | Apple Silicon (aarch64) | ARM64 AAPCS64 Darwin ABI | Apple Clang (Xcode 14+) | Supported via cross-target & LLVM |
| **Tier 2 (Secondary)** | macOS 12+ (Monterey, Sonoma) | x86_64 | System V AMD64 Darwin ABI | Apple Clang (Xcode 14+) | Supported via cross-target & LLVM |
| **Tier 2 (Secondary)** | Linux (Alpine, RHEL, Ubuntu) | aarch64 (ARM64) | ARM64 AAPCS64 Linux ABI | GCC 12+ / Clang 15+ (glibc / musl) | Cross-compilation verified |

---

## 2. Subsystem Parity Invariants

### 2.1 Compiler Code Generation Backends

| Subsystem / Feature | Native x86_64 Backend | LLVM IR Emitter | Compatibility & Parity Notes |
|---|---|---|---|
| **Calling Conventions** | Internal Fast ABI, Win64, SysV | Standard LLVM `ccc` & fastcc | Automatic parameter and return register assignment |
| **SSA Dominance & Phi** | Split-edge parallel copy resolver | LLVM `phi` nodes | Parallel copies break cyclic dependencies via scratch staging |
| **Register Allocation** | Linear Scan (Poletto & Sarkar) | LLVM Greedy / Fast RegAlloc | Callee-saved preservation & spill slots reuse |
| **Vector / SIMD** | Direct SSE/AVX assembly emission | LLVM vector primitives | `f32x4`, `i32x4`, `f64x2`, `i64x2` full parity |
| **Debug Info** | DWARF 5 / CodeView line annotations | LLVM DIBuilder metadata | Dual debug information emission supported |

---

### 2.2 Runtime Platform Abstraction Layer (PAL)

| Runtime Subsystem | Windows x86_64 Implementation | Linux & POSIX Implementation |
|---|---|---|
| **Threading** | Win32 API (`CreateThread`, `WaitForSingleObject`, `CloseHandle`) | POSIX Threads (`pthread_create`, `pthread_join`, `pthread_detach`) |
| **Mutual Exclusion** | Win32 `CRITICAL_SECTION` (`EnterCriticalSection`, `LeaveCriticalSection`) | POSIX Mutex (`pthread_mutex_lock`, `pthread_mutex_unlock`) |
| **Fibers & Coroutines** | Windows Fiber API (`ConvertThreadToFiber`, `CreateFiber`, `SwitchToFiber`) | POSIX `ucontext_t` (`makecontext`, `swapcontext`, stack bounds) |
| **Event Loop Polling** | Winsock non-blocking polling (`select`, `Sleep`) with GUI hook pump | Linux `epoll` (`epoll_create1`, `epoll_ctl`, `epoll_wait`) / `poll` |
| **High-Res Timers** | `QueryPerformanceCounter` / `GetSystemTimeAsFileTime` | `clock_gettime(CLOCK_MONOTONIC)` |
| **Network Sockets** | Winsock2 (`WSAStartup`, `WSACleanup`, `closesocket`, `ioctlsocket`) | Berkeley Sockets (`socket`, `close`, `fcntl(O_NONBLOCK)`) |
| **Thread-Local Storage** | `__declspec(thread)` / `TlsAlloc` | `__thread` / `pthread_key_create` |
| **Slab Memory Cache** | Thread-local slab cache, 16-byte alignment, freed history tracking | Thread-local slab cache, 16-byte alignment, freed history tracking |

---

## 3. Toolchain & Build Requirements

To compile and verify SantientPHP v0.2.1 from source:

- **CMake**: Version 3.20 or newer
- **Ninja**: Version 1.10 or newer (preferred generator)
- **C++ Compiler**: C++20 compliant compiler
  - Windows: MinGW-w64 GCC 12.0+ (UCRT64) or MSVC 19.30+ (Visual Studio 2022)
  - Linux: GCC 12.0+ or Clang 15.0+
- **System Libraries**:
  - Windows: `ws2_32`, `comctl32`, `gdi32`, `user32`, `opengl32`, `ole32`, `winmm`, `winhttp`
  - Linux: `pthread`, `m`, `dl` (and optional `libsqlite3` if building embedded PDO SQLite)

