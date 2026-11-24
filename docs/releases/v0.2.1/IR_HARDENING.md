# SantientPHP v0.2.1 — SPHIR Correctness & Optimizer Hardening Report

**Codename:** Compiler Reliability & IR Hardening  
**Scope:** IR Invariants, Instruction Parenting, Centralized Basic Block APIs, and Verifier Diagnostic Architecture  

---

## 1. Executive Summary

During optimization passes in early development builds of SantientPHP, intermediate transformation passes constructed or moved instructions without explicitly binding their `m_parent` pointer to the target `IRBasicBlock`. When `IRVerifier` subsequently checked instructions and phi incoming values, instructions with `nullptr` parent pointers triggered false-positive cross-function reference violations:
```text
IR verification failed after optimization:
  cross-function value reference in phi instruction of function '@run_matrix'
  cross-function operand reference in function '@run_matrix'
```

In v0.2.1, this vulnerability was resolved through a comprehensive four-layer defense:
1. **Centralized Basic Block Manipulation APIs** on `IRBasicBlock`.
2. **Optimizer Pass Auditing and Refactoring** across all six affected passes.
3. **Strengthened IR Verifier Diagnostics** validating parent pointers and block hierarchy.
4. **Per-Pass Verification Mode** in `PassManager` for immediate isolation of regression passes.

---

## 2. Centralized Instruction Manipulation APIs

Previously, optimization passes manipulated instructions directly through raw `std::vector<std::shared_ptr<IRInstruction>>&` access:
```cpp
// Flawed legacy pattern:
instructions[i] = std::make_shared<BranchInst>(target); // inst->m_parent remains nullptr!
```

To eliminate manual parent management, `IRBasicBlock` now exposes centralized manipulation primitives that guarantee parent pointer assignment and cleanup:

```cpp
class IRBasicBlock {
public:
    void addInstruction(std::shared_ptr<IRInstruction> inst);
    void appendInstruction(std::shared_ptr<IRInstruction> inst);
    void insertInstruction(size_t index, std::shared_ptr<IRInstruction> inst);
    void replaceInstruction(size_t index, std::shared_ptr<IRInstruction> inst);
    void removeInstruction(size_t index);
    void moveInstructionTo(IRBasicBlock& targetBlock, size_t fromIdx, size_t toIdx);
...
};
```

### Invariants Enforced by Centralized Operations
- **`insertInstruction(idx, inst)` / `appendInstruction(inst)`**: Invokes `inst->setParent(this)` and preserves instruction ordering.
- **`replaceInstruction(idx, inst)`**: Invokes `inst->setParent(this)` on the new instruction and clears `oldInst->setParent(nullptr)`.
- **`removeInstruction(idx)`**: Safely resets `inst->setParent(nullptr)` before erasing the element, preventing stale parent dangling references.
- **`moveInstructionTo(target, fromIdx, toIdx)`**: Atomically transfers the instruction from `this` to `target`, rebinding `inst->setParent(&target)`.

---

## 3. Audit of the Six Optimization Passes

| Optimization Pass | Source File | Centralized API Adopted | Invariant Guarantee |
|---|---|---|---|
| **Mem2RegPass** | `compiler/optimizer/Mem2RegPass.cpp` | `bb->insertInstruction(insertIdx, phi)` | Phi instructions inserted into iterated dominance frontier ($\text{IDF}$) blocks immediately receive their parent pointer upon insertion. |
| **FunctionInliningPass** | `compiler/optimizer/FunctionInliningPass.cpp` | `callerBlock->removeInstruction(idx)` + `callerBlock->insertInstruction(idx + k, cloned)` | Cloned instructions inlined into caller blocks are bound to the caller block, and replaced `CallInst` is dissociated safely. |
| **EscapeAnalysisPass** | `compiler/optimizer/EscapeAnalysisPass.cpp` | `entryBlock->insertInstruction(insertPos, allocaInst)` | Promoted stack allocas inserted into function entry blocks are bound to the entry block before replacing heap call uses. |
| **ConstantFoldingPass** | `compiler/optimizer/ConstantFoldingPass.cpp` | `bb->replaceInstruction(i, std::make_shared<BranchInst>(target))` | Folded conditional branches replaced with unconditional branches are assigned parent block ownership automatically. |
| **ControlFlowOptimizationPass** | `compiler/optimizer/ControlFlowOptimizationPass.cpp` | `bb->replaceInstruction(lastIdx, std::make_shared<BranchInst>(target))` | Folded branch instructions targeting identical successors are safely replaced and bound to the block. |
| **LoopInvariantCodeMotionPass** | `compiler/optimizer/LoopInvariantCodeMotionPass.cpp` | `preheader->insertInstruction(insertPos++, inv)` | Hoisted loop-invariant instructions moved to loop preheaders have their parent updated before preheader terminators. |

---

## 4. Strengthened SPHIR Verifier Diagnostics

`compiler/ir/IRVerifier.cpp` was extended to enforce structural hierarchy invariants:

1. **Basic Block Parent Ownership**:
   ```cpp
   if (bb->getParent() != &fn) {
       errors.push_back("basic block '" + bb->getName() + "' parent pointer does not match function '@" + fn.getName() + "'");
       ok = false;
   }
   ```
2. **Instruction Parent Ownership**:
   ```cpp
   if (!inst->getParent()) {
       errors.push_back("instruction in block '" + bb.getName() + "' of function '@" + fn.getName() + "' has null parent basic block pointer");
       ok = false;
   } else if (inst->getParent() != &bb) {
       errors.push_back("instruction in block '" + bb.getName() + "' has mismatched parent pointer pointing to block '" + inst->getParent()->getName() + "'");
       ok = false;
   }
   ```
3. **Cross-Function Value & Phi Edge Isolation**:
   Operands and incoming phi values that are instructions must satisfy `defInst->getParent() && defInst->getParent()->getParent() == &fn`.
4. **Dominance Conformance**:
   Instruction uses must be strictly dominated by definitions along all CFG paths.

---

## 5. Diagnostic Per-Pass Verification Architecture

`PassManager` now features diagnostic per-pass verification:

```cpp
pm.setVerifyEachPass(true);
pm.run(module);
```

When enabled, `PassManager::run` executes `IRVerifier::verify` immediately after each module pass and each function pass. Any invariant violation aborts and prints the exact pass name, basic block, instruction, and invariant failure.

In addition, `PassManager::run` retains an end-of-pipeline parent synchronization sweep as defense-in-depth, ensuring release builds remain completely shielded against downstream code-generator panics.

---

## 6. Acceptance Criteria Verification

- [x] All six previously affected passes preserve parent relationships.
- [x] Each optimization pass can be independently verified.
- [x] No cross-function operand or phi reference errors occur at `-O1`, `-O2`, or `-O3`.
- [x] Centralized insertion/movement APIs guarantee parent pointer consistency.
- [x] Full regression test suite passing with 0 failures.

