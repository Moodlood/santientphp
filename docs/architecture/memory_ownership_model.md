# SantientPHP Memory Ownership Model & Architecture Specification

## 1. Overview & Memory Domains

SantientPHP combines high-performance native ahead-of-time (AOT) compilation with predictable memory safety. The language unifies eight distinct memory domains into a cohesive ownership hierarchy:

1. **Stack Storage (`AllocaInst`)**: Deterministic frame-bounded lifetimes for local values and non-escaping structures.
2. **Deterministic Heap Box (`Box<T>`)**: Single-owner heap allocations deallocated deterministically when leaving lexical scope.
3. **Reference-Counted Smart Pointers (`Rc<T>`)**: Shared immutable or interior-mutable heap allocations managed via atomic/local ref-counts.
4. **Arena Allocators (`Arena`)**: High-throughput regional bulk allocators freed in $O(1)$ on arena destruction.
5. **Thread-Local Slab Cache (`sph_alloc` <= 64 bytes)**: Micro-allocations served from thread-local bump pointers and free lists with 16-byte alignment.
6. **Binary Buffers (`ByteBuffer`)**: Fixed or dynamically sliced byte sequences safe for arbitrary null-byte payloads.
7. **Dynamic PHP Values (`SphValue`)**: Tagged 16-byte unions with copy-on-write semantics for PHP compatibility.
8. **Foreign Function Interface (`unsafe`, `*T`)**: Raw unmanaged memory interoperating with external C ABIs.

---

## 2. Ownership & Lifetime Semantics

| Domain | Allocation Primitive | Deallocation Primitive | Ownership Semantic | Thread Safety |
|---|---|---|---|---|
| **Stack** | `AllocaInst` | Frame Epilogue (`add rsp`) | Unique to stack frame | Thread-isolated |
| **Box<T>** | `sph_alloc` | `sph_free` | Unique (move semantics) | Movable across threads |
| **Rc<T>** | `sph_rc_alloc` | `sph_rc_release` | Shared ownership | Thread-local count |
| **Arena** | `sph_arena_alloc` | `sph_arena_destroy` | Region-owned | Per-arena isolation |
| **Slab** | `sph_alloc` (<=64B) | `sph_free` | Slab-managed block | Thread-local cache |
| **ByteBuffer** | `sph_buffer_alloc` | `sph_buffer_free` | Managed buffer header | Movable |
| **SphValue** | Heap/Inline | Reference/Value drop | Copy-On-Write | Single-thread COW |
| **Raw Pointers** | External / Manual | User-directed | Unchecked raw | Unchecked |

---

## 3. Escape Analysis & Stack Demotion

### 3.1 Escape Invariants

The `EscapeAnalysisPass` analyzes functions in SSA form to identify heap allocations (`sph_alloc`) that do not outlive the executing stack frame. An allocation `A` is said to **escape** if any of the following conditions holds:

1. **Return Escape**: `A` or any transitive alias of `A` is returned via `RetInst`.
2. **Store Escape**: `A` or any transitive alias is stored as a value into a heap location, global variable, or another struct field (`StoreInst::getValue()`).
3. **Call / Concurrency Escape**: `A` or any alias is passed as an argument to any function other than `sph_free` (e.g. `sph_thread_spawn`, `sph_fiber_spawn`, `sph_event_loop_spawn`, or external C ABI calls).
4. **Pointer Escape**: `A` undergoes `PtrToInt` casting or unconstrained pointer arithmetic.
5. **Loop Escape**: The allocation occurs inside a basic block that is part of a cyclic path / loop in the control-flow graph (CFG). Demoting loop allocations to entry `AllocaInst` would unsafely alias storage across iterations.

### 3.2 Transitive Alias Closure

The compiler computes the transitive alias closure over `CastInst`, `BitCastInst`, `GetElementPtrInst`, `IntToPtrInst`, and `PhiInst`. If any element in the alias closure escapes, the underlying `sph_alloc` remains on the heap.

When an allocation is verified not to escape:
- `sph_alloc` is replaced by an `AllocaInst` placed at the entry basic block of the function.
- All uses of `sph_alloc` are rebound to the `AllocaInst`.
- All corresponding `sph_free` instructions targeting the allocation are safely removed.

---

## 4. Slab Allocator Architecture & Reliability

### 4.1 Block Layout & 16-Byte Alignment

Small allocations ($\le 64$ bytes) are handled by the thread-local slab allocator. Each block consists of a 16-byte metadata header followed by a 64-byte payload:

```
+-------------------+-------------------+-----------------------------+
| magic (uint32_t)  | size (uint32_t)   | next (struct sph_slab_block*) |
+-------------------+-------------------+-----------------------------+
<------------------------ 16-byte Header ----------------------------->
+---------------------------------------------------------------------+
| Payload Memory (64 bytes, 16-byte aligned)                          |
+---------------------------------------------------------------------+
```

- `magic`: Set to `SPH_SLAB_MAGIC` (`0x53504853`, `"SPHS"`) when active.
- `size`: Allocation size requested by caller.
- `next`: Free-list pointer used when the block is pooled in `g_slab.free_pool`.

### 4.2 Large Allocation Header

Allocations larger than 64 bytes use `malloc` with a prefixed 16-byte header:

```
+-------------------+-------------------+-----------------------------+
| magic (uint32_t)  | size (uint32_t)   | _pad (uint64_t)             |
+-------------------+-------------------+-----------------------------+
```

- `magic`: Set to `SPH_LARGE_MAGIC` (`0x4C415247`, `"LARG"`).

### 4.3 Double-Free Detection

Upon deallocation (`sph_free`):
1. The allocator inspects `((sph_slab_block*)ptr) - 1`.
2. If `magic == SPH_SLAB_MAGIC`, it is transitioned to `SPH_SLAB_FREED_MAGIC` (`0x44454144`, `"DEAD"`) and linked into `g_slab.free_pool`.
3. If `magic == SPH_SLAB_FREED_MAGIC`, a **double free** is detected and rejected without corrupting the freelist or heap.
4. For large allocations, if `magic == SPH_LARGE_MAGIC`, it transitions to `SPH_LARGE_FREED_MAGIC` (`0x4C444541`, `"LDEA"`). A second free is safely detected and rejected.

---

## 5. Memory Debugging & Tracking API

The runtime provides real-time allocation accounting via atomic counters:

```c
int64_t sph_alloc_count(void);      // Total number of allocations performed
int64_t sph_free_count(void);       // Total number of successful deallocations
int64_t sph_current_bytes(void);    // Current live allocated bytes
int64_t sph_peak_bytes(void);       // Peak live allocated bytes
void    sph_mem_reset_stats(void);   // Reset tracking counters
void    sph_mem_dump_leaks(void);    // Emit formatted leak summary if live bytes > 0
int32_t sph_mem_validate_ptr(void*); // Verify if pointer is active slab or large block
```

---

## 6. FFI & Cross-Boundary Ownership Rules

When interacting with external C libraries via `extern "C"`:
1. Pointers allocated via `sph_alloc` passed to C functions remain valid until explicitly freed with `sph_free`.
2. C functions must not invoke `free()` directly on pointers allocated with `sph_alloc`, as slab headers would be corrupted.
3. Memory allocated by external C libraries (e.g. `malloc`) passed into SantientPHP must be freed using the corresponding C library deallocator or passed to `sph_free` only if untracked raw fallback is configured.

