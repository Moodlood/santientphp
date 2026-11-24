# SantientPHP v0.2.1 — Engineering Walkthrough: Compiler Reliability, IR Hardening & Runtime Optimization

## 1. Overview & Objectives

During the stabilization and verification phase of **SantientPHP v0.2.1 ("Compiler Reliability & Runtime Hardening")**, running the multi-language benchmark suite (`tools/benchmarks/run_benchmarks.py`) revealed two critical engineering challenges:

1. **Compiler IR Verification Failures at Optimization Levels `-O1` through `-O3`**:
   - `Matrix Multi (200x200)`, `String Concat (50k)`, and `Heap Alloc (1M items)` failed compilation with errors such as:
     - `cross-function operand reference in function '@run_matrix'`
     - `cross-function value reference in phi instruction of function '@run_matrix'`
     - `cross-function operand reference in function '@main'`
2. **Runtime Memory Allocation Overhead in High-Churn Scenarios**:
   - In `Heap Alloc (1M items)`, executing 1,000,000 allocations and deallocations took **2,752.57 ms** due to linear memory-safety scan loops in the runtime allocator.

This walkthrough documents the root causes discovered, the fixes implemented across the optimizer, compiler driver, and runtime, and the resulting performance gains.

---

## 2. Investigation: IR Verification Failures

### 2.1 The Symptom
When executing `sphc build benchmarks/matrix_mult/matmul.sph -O3`, the compiler aborted with 9 verification errors:
```text
IR verification failed after optimization (9 errors):
  cross-function value reference in phi instruction of function '@run_matrix'
  cross-function operand reference in function '@run_matrix'
  ...
```
Similarly, `strings.sph` and `alloc.sph` failed with 3 and 5 cross-function operand errors respectively.

### 2.2 Root Cause Analysis
By checking compiler pipeline execution across `-O0`, `-O1`, `-O2`, and `-O3`:
- `-O0` succeeded without errors.
- `-O1`, `-O2`, and `-O3` all failed with identical errors.

In `compiler/ir/IRVerifier.cpp`, the verifier enforces:
```cpp
// For Phi incoming values:
if (inVal->isInstruction()) {
    const auto* defInst = static_cast<const IRInstruction*>(inVal.get());
    if (!defInst->getParent() || defInst->getParent()->getParent() != &fn) {
        errors.push_back("cross-function value reference in phi instruction of function '@" + fn.getName() + "'");
    }
}

// For Non-Phi operands:
if (op->isInstruction()) {
    const auto* defInst = static_cast<const IRInstruction*>(op);
    if (!defInst->getParent() || defInst->getParent()->getParent() != &fn) {
        errors.push_back("cross-function operand reference in function '@" + fn.getName() + "'");
    }
}
```
Notice the condition: `!defInst->getParent() || defInst->getParent()->getParent() != &fn`.

When optimization passes constructed new instructions and inserted them directly into `IRBasicBlock::getInstructions()`, they omitted calling `inst->setParent(bb)`. As a result, `defInst->getParent()` remained `nullptr`. The verifier interpreted `!defInst->getParent()` as a cross-function reference.

### 2.3 Affected Optimization Passes
An audit of `compiler/optimizer/` revealed missing parent assignments across six optimization passes:

| File | Location | Missing Invariant |
|---|---|---|
| `Mem2RegPass.cpp` | Dominance frontier $\text{IDF}$ insertion | `phi->setParent(bb)` not called when inserting phi nodes into loop headers. |
| `FunctionInliningPass.cpp` | Instruction cloning & insertion | Cloned instructions inserted into caller lacked `cloned->setParent(callerBlock.get())`. |
| `EscapeAnalysisPass.cpp` | Stack alloca promotion | `allocaInst->setParent(entryBlock)` omitted when replacing heap `sph_alloc` calls. |
| `ConstantFoldingPass.cpp` | Branch simplification | `instructions[i]->setParent(bb.get())` omitted when converting constant `CondBr` to `Br`. |
| `ControlFlowOptimizationPass.cpp` | Branch folding | `insts.back()->setParent(bb.get())` omitted when replacing identical branch targets. |
| `LoopInvariantCodeMotionPass.cpp` | Preheader hoisting | `inv->setParent(loop.preheader)` omitted when moving loop-invariant instructions to preheaders. |

---

## 3. Engineering Fixes

### 3.1 Optimizer Instruction Parenting
Each pass was updated to guarantee that all newly created, cloned, or relocated instructions have their parent basic block explicitly assigned:

1. **`Mem2RegPass.cpp`**:
   ```cpp
   auto phi = std::make_shared<PhiInst>(
       alloca->getAllocatedType(),
       alloca->getName().empty() ? "phi" : (alloca->getName() + ".phi"));
   phi->setParent(bb); // Initialized parent pointer
   placedPhis[alloca][bb] = phi;
   ```
2. **`EscapeAnalysisPass.cpp`**:
   ```cpp
   auto allocaInst = std::make_shared<AllocaInst>(innerType, call->getName());
   allocaInst->setParent(entryBlock);
   ```
3. **`ConstantFoldingPass.cpp` & `ControlFlowOptimizationPass.cpp`**:
   ```cpp
   instructions[i] = std::make_shared<BranchInst>(target);
   instructions[i]->setParent(bb.get());
   ```
4. **`LoopInvariantCodeMotionPass.cpp`**:
   ```cpp
   for (const auto& inv : invariantInsts) {
       inv->setParent(loop.preheader);
       preInsts.push_back(inv);
   }
   ```

### 3.2 Inliner Correctness Hardening (`FunctionInliningPass.cpp`)
To eliminate unmapped instruction references and prevent invalid inlining:
1. **Instruction Cloneability Whitelist**: Added `isCloneableInstruction` ensuring that functions are only considered inline candidates if every instruction within them can be safely cloned (handling arithmetic, logic, casts, GEP, memory access, `Ret`, `RcAlloc`, and `RcRetain`).
2. **Parameter-to-Argument Arity Verification**: Enforced `params.size() == callInst->getArgs().size()`.
3. **Cloned Instruction Parenting**:
   ```cpp
   if (cloned) {
       cloned->setParent(callerBlock.get());
       valueMap[cInst.get()] = cloned;
       inlinedInsts.push_back(cloned);
   }
   ```

### 3.3 Defense-in-Depth Parent Synchronization (`PassManager.cpp`)
At the end of `PassManager::run(IRModule& module)`, an invariant-enforcement sweep was added to guarantee that every instruction in every basic block has its parent pointer synced prior to downstream verification or code generation:
```cpp
for (auto& fn : module.getFunctions()) {
    if (fn->isDeclaration()) continue;
    for (auto& bb : fn->getBlocks()) {
        if (!bb) continue;
        for (auto& inst : bb->getInstructions()) {
            if (inst) {
                inst->setParent(bb.get());
            }
        }
    }
}
```

---

## 4. Runtime Memory Allocator Optimization

### 4.1 Bottleneck in `runtime/core/core.c`
In the `Heap Alloc (1M items)` benchmark, SantientPHP took **2,752.57 ms**, running slower than PHP 8.2 (886.5 ms). Profiling `runtime/core/core.c` revealed:
- `sph_alloc` called `remove_from_freed_history(ptr)` which scanned a 256-element array linearly.
- `sph_free` called `is_in_freed_history(ptr)` which scanned the 256-element array linearly on every deallocation.
- For 1,000,000 items, this resulted in **$1,000,000 \times 256 \times 2 = 512,000,000$ loop iterations**.

### 4.2 The O(1) Fast-Path Architecture
1. **Direct-Mapped Hash Table**:
   Replaced the 256-element linear array with a power-of-two hash table ($512$ slots) using bitwise masking:
   ```c
   #define SPH_FREED_HISTORY_SIZE 512
   #define SPH_FREED_HISTORY_MASK (SPH_FREED_HISTORY_SIZE - 1)
   static void* g_freed_history[SPH_FREED_HISTORY_SIZE] = {0};

   static inline size_t hash_freed_ptr(void* ptr) {
       uintptr_t v = (uintptr_t)ptr;
       v ^= v >> 16;
       v ^= v >> 8;
       return (size_t)(v & SPH_FREED_HISTORY_MASK);
   }

   static inline int is_in_freed_history(void* ptr) {
       if (!ptr) return 0;
       return g_freed_history[hash_freed_ptr(ptr)] == ptr;
   }
   ```
2. **Slab Allocation Fast-Path**:
   For slab blocks ($\le 512$ bytes), double-free detection is already guaranteed in $O(1)$ by the block magic (`blk->magic == SPH_SLAB_FREED_MAGIC`). We moved the slab magic check to the top of `sph_free` before querying the history table:
   ```c
   void sph_free(void* ptr) {
       if (!ptr) return;

       // Fast path: Check slab block magic first without history scan
       sph_slab_block* blk = ((sph_slab_block*)ptr) - 1;
       if (blk->magic == SPH_SLAB_MAGIC) {
           blk->magic = SPH_SLAB_FREED_MAGIC;
           __atomic_fetch_add(&g_free_count, 1, __ATOMIC_RELAXED);
           __atomic_fetch_sub(&g_current_bytes, (int64_t)blk->size, __ATOMIC_RELAXED);
           blk->next = g_slab.free_pool;
           g_slab.free_pool = blk;
           return;
       }
       if (blk->magic == SPH_SLAB_FREED_MAGIC) {
           // Double free on slab block detected: ignore safely
           return;
       }

       if (is_in_freed_history(ptr)) return;
       ...
   }
   ```

### 4.3 Benchmark Impact
- **Heap Alloc (1M items)** execution time dropped from **2,752.57 ms to 387.26 ms** (**7.1x speedup**, **2,365 ms shaved off**).
- SantientPHP is now **1.9x faster than PHP 8.2** on heap allocations.
- **String Concat (50k)** dropped from **82.11 ms to 66.56 ms** (outperforming Go's 90.63 ms).

---

## 5. Benchmark Performance Comparison

All four benchmarks now compile and run cleanly at `-O3` with reproducible output in `benchmarks/results.json`:

```text
===================================================================================================================
 SUMMARY TABLE (Execution Time in milliseconds, lower is faster)
===================================================================================================================
Benchmark                | SantientPHP  | C (GCC -O3)  | Rust (-O3)   | Go         | PHP 8.2    | SPH vs PHP   | SPH vs Rust
-------------------------------------------------------------------------------------------------------------------
Fibonacci (N=35)         | 472.99 ms    | 120.95 ms    | 387.03 ms    | 391.69 ms  | 6489.6 ms  | 13.7x faster | 1.2x slower
Matrix Multi (200x200)   | 170.23 ms    | 24.33 ms     | 24.91 ms     | 32.79 ms   | 1456.13 ms | 8.6x faster  | 6.8x slower
String Concat (50k)      | 66.56 ms     | 22.56 ms     | 24.0 ms      | 90.63 ms   | 249.96 ms  | 3.8x faster  | 2.8x slower
Heap Alloc (1M items)    | 387.26 ms    | 20.4 ms      | 25.27 ms     | 20.03 ms   | 734.26 ms  | 1.9x faster  | 15.3x slower
===================================================================================================================
```

- **Regression Test Suite**: **6,625 tests passed (0 failures)**.

---

## 6. Next Performance Roadmap

Further performance improvements can be achieved without altering any language features:

1. **Windows x64 Non-Volatile Register Expansion**:
   Add `rsi` and `rdi` to `calleeSavedRegs` in `LinearScanAllocator.cpp`. This expands the callee-saved pool from 5 to 7 registers, preventing loop variable spills in `Matrix Multi` and eliminating over 32,000,000 stack memory operations.
2. **Direct-Register Instruction Selection**:
   In `X86_64CodeGen.cpp`, eliminate intermediate funneling through `rax`/`rcx` for binary operations (`mov rax, r14; mov rcx, 1; add rax, rcx; mov r11, rax` $\to$ `add r11, 1`), shaving ~150 ms off recursive calls like Fibonacci.
3. **Loop-Scoped Alloca Reuse in Escape Analysis**:
   Permit `EscapeAnalysisPass` to demote allocations whose lifetimes are strictly iteration-local (`Box::new(...)` inside a loop), bringing Heap Alloc from 387 ms down to ~25 ms.

