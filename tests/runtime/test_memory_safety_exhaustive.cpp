#include "common/test_framework.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include "compiler/optimizer/EscapeAnalysisPass.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

extern "C" {
    void* sph_alloc(int64_t size);
    void sph_free(void* ptr);
    int64_t sph_alloc_count(void);
    int64_t sph_free_count(void);
    int64_t sph_current_bytes(void);
    int64_t sph_peak_bytes(void);
    void sph_mem_reset_stats(void);
    void sph_mem_dump_leaks(void);
    int32_t sph_mem_validate_ptr(void* ptr);
}

namespace sph {

TEST_CASE("Memory Safety: Slab double-free detection prevents heap corruption") {
    sph_mem_reset_stats();
    void* ptr = sph_alloc(32);
    CHECK(ptr != nullptr);
    CHECK_EQ(sph_mem_validate_ptr(ptr), 1);

    int64_t allocs = sph_alloc_count();
    CHECK_EQ(allocs, 1);

    sph_free(ptr);
    CHECK_EQ(sph_free_count(), 1);
    CHECK_EQ(sph_current_bytes(), 0);
    CHECK_EQ(sph_mem_validate_ptr(ptr), 0);

    // Double free attempt must be safely intercepted without crashing
    sph_free(ptr);
    CHECK_EQ(sph_free_count(), 1); // Counter did not double-increment

    // Allocate next block: freelist must remain valid
    void* ptr2 = sph_alloc(32);
    CHECK(ptr2 != nullptr);
    CHECK_EQ(sph_mem_validate_ptr(ptr2), 1);
    sph_free(ptr2);
}

TEST_CASE("Memory Safety: Large allocation fallback with 16-byte alignment and double-free detection") {
    sph_mem_reset_stats();
    void* largePtr = sph_alloc(1024);
    CHECK(largePtr != nullptr);
    CHECK_EQ(reinterpret_cast<uintptr_t>(largePtr) % 16, 0u);
    CHECK_EQ(sph_mem_validate_ptr(largePtr), 1);
    CHECK_EQ(sph_current_bytes(), 1024);

    // Verify writable
    std::memset(largePtr, 0xEE, 1024);
    CHECK_EQ(static_cast<uint8_t*>(largePtr)[0], 0xEE);
    CHECK_EQ(static_cast<uint8_t*>(largePtr)[1023], 0xEE);

    sph_free(largePtr);
    CHECK_EQ(sph_free_count(), 1);
    CHECK_EQ(sph_current_bytes(), 0);
    CHECK_EQ(sph_mem_validate_ptr(largePtr), 0);

    // Large double free attempt
    sph_free(largePtr);
    CHECK_EQ(sph_free_count(), 1);
}

TEST_CASE("Memory Safety: Allocation stats and peak memory tracking") {
    sph_mem_reset_stats();
    CHECK_EQ(sph_alloc_count(), 0);
    CHECK_EQ(sph_free_count(), 0);
    CHECK_EQ(sph_current_bytes(), 0);

    std::vector<void*> blocks;
    for (int i = 0; i < 10; ++i) {
        blocks.push_back(sph_alloc(50));
    }
    CHECK_EQ(sph_alloc_count(), 10);
    CHECK_EQ(sph_current_bytes(), 500);
    CHECK(sph_peak_bytes() >= 500);

    for (void* p : blocks) {
        sph_free(p);
    }
    CHECK_EQ(sph_free_count(), 10);
    CHECK_EQ(sph_current_bytes(), 0);
    CHECK(sph_peak_bytes() >= 500);
}

TEST_CASE("Escape Analysis Hardening: Transitive alias returned prevents stack demotion") {
    auto ptrType = PointerType::get(Type::getVoid());
    IRFunction fn("fn_ea_transitive_ret", ptrType);

    auto bb = fn.createBlock("entry");
    auto sz = ConstantInt::get(64, Type::getI64());
    auto allocCall = std::make_shared<CallInst>("sph_alloc", std::vector<std::shared_ptr<IRValue>>{sz}, ptrType, "heapMem");
    bb->addInstruction(allocCall);

    auto intPtrType = PointerType::get(Type::getI32());
    auto castInst = std::make_shared<CastInst>(allocCall, intPtrType, "derivedCast");
    bb->addInstruction(castInst);

    // Returning the derived pointer escapes the allocation
    bb->addInstruction(std::make_shared<RetInst>(castInst));

    EscapeAnalysisPass pass;
    bool changed = pass.runOnFunction(fn);
    CHECK(!changed); // Must NOT demote
}

TEST_CASE("Escape Analysis Hardening: Allocation stored into memory escapes") {
    auto ptrType = PointerType::get(Type::getVoid());
    IRFunction fn("fn_ea_store_escape", Type::getVoid());

    auto bb = fn.createBlock("entry");
    auto targetMem = std::make_shared<AllocaInst>(ptrType, "storageSlot");
    bb->addInstruction(targetMem);

    auto sz = ConstantInt::get(32, Type::getI64());
    auto allocCall = std::make_shared<CallInst>("sph_alloc", std::vector<std::shared_ptr<IRValue>>{sz}, ptrType, "valToEscape");
    bb->addInstruction(allocCall);

    // Storing the allocated pointer into storageSlot causes it to escape
    bb->addInstruction(std::make_shared<StoreInst>(allocCall, targetMem));
    bb->addInstruction(std::make_shared<RetInst>());

    EscapeAnalysisPass pass;
    bool changed = pass.runOnFunction(fn);
    CHECK(!changed); // Must NOT demote
}

TEST_CASE("Escape Analysis Hardening: Passing allocation to thread spawn escapes") {
    auto ptrType = PointerType::get(Type::getVoid());
    IRFunction fn("fn_ea_thread_escape", Type::getVoid());

    auto bb = fn.createBlock("entry");
    auto sz = ConstantInt::get(64, Type::getI64());
    auto allocCall = std::make_shared<CallInst>("sph_alloc", std::vector<std::shared_ptr<IRValue>>{sz}, ptrType, "threadPayload");
    bb->addInstruction(allocCall);

    auto spawnCall = std::make_shared<CallInst>(
        "sph_thread_spawn",
        std::vector<std::shared_ptr<IRValue>>{allocCall},
        Type::getI64(),
        "threadId");
    bb->addInstruction(spawnCall);
    bb->addInstruction(std::make_shared<RetInst>());

    EscapeAnalysisPass pass;
    bool changed = pass.runOnFunction(fn);
    CHECK(!changed); // Must NOT demote
}

TEST_CASE("Escape Analysis Hardening: Allocation inside loop prevents unsafe stack demotion") {
    auto ptrType = PointerType::get(Type::getVoid());
    IRFunction fn("fn_ea_loop_alloc", Type::getVoid());

    auto entry = fn.createBlock("entry");
    auto loopHeader = fn.createBlock("loop.header");
    auto loopBody = fn.createBlock("loop.body");
    auto exit = fn.createBlock("exit");

    entry->addInstruction(std::make_shared<BranchInst>(loopHeader));

    auto cond = ConstantInt::get(1, Type::getBool());
    loopHeader->addInstruction(std::make_shared<CondBranchInst>(cond, loopBody, exit));

    auto sz = ConstantInt::get(32, Type::getI64());
    auto allocCall = std::make_shared<CallInst>("sph_alloc", std::vector<std::shared_ptr<IRValue>>{sz}, ptrType, "loopAlloc");
    loopBody->addInstruction(allocCall);

    auto freeCall = std::make_shared<CallInst>("sph_free", std::vector<std::shared_ptr<IRValue>>{allocCall}, Type::getVoid(), "freeCall");
    loopBody->addInstruction(freeCall);
    loopBody->addInstruction(std::make_shared<BranchInst>(loopHeader));

    exit->addInstruction(std::make_shared<RetInst>());

    EscapeAnalysisPass pass;
    bool changed = pass.runOnFunction(fn);
    CHECK(!changed); // Must NOT demote allocation inside cyclic loop
}

} // namespace sph

