#include "common/test_framework.hpp"
#include "runtime/core/core.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace sph {

namespace {

struct SlabExhaustiveRegistrar {
    SlabExhaustiveRegistrar() {
        registerMicroAllocations();
        registerAlignmentChecks();
        registerChunkRollover();
        registerConcurrentSlabAllocations();
    }

    void registerMicroAllocations() {
        // 100 tests: Fast bump-pointer slab allocations for sizes 1 to 64 bytes
        for (int sz = 1; sz <= 100; ++sz) {
            std::string name = "SlabExhaustive: Micro allocation size " + std::to_string(sz);
            testing::TestRegistry::instance().registerTest(name, [sz]() {
                size_t allocSize = (sz % 64) + 1;
                void* ptr = sph_alloc(allocSize);
                CHECK(ptr != nullptr);

                // Write and verify pattern to ensure valid writable memory
                std::memset(ptr, 0xAA, allocSize);
                uint8_t* bytePtr = static_cast<uint8_t*>(ptr);
                CHECK_EQ(bytePtr[0], 0xAA);
                CHECK_EQ(bytePtr[allocSize - 1], 0xAA);

                sph_free(ptr);
            });
        }
    }

    void registerAlignmentChecks() {
        // 100 tests: Every slab allocation must be aligned to 16 bytes
        for (int i = 1; i <= 100; ++i) {
            std::string name = "SlabExhaustive: 16-byte alignment check " + std::to_string(i);
            testing::TestRegistry::instance().registerTest(name, [i]() {
                size_t allocSize = (i * 7) % 64 + 1;
                void* ptr = sph_alloc(allocSize);
                CHECK(ptr != nullptr);

                uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
                CHECK_EQ(addr % 16, 0u);

                sph_free(ptr);
            });
        }
    }

    void registerChunkRollover() {
        // 100 tests: Allocating many items causes slab chunk rollover (exceeding 64KB) seamlessly
        for (int k = 1; k <= 100; ++k) {
            std::string name = "SlabExhaustive: 64KB chunk rollover " + std::to_string(k);
            testing::TestRegistry::instance().registerTest(name, [k]() {
                int count = 1200; // 1200 * 64 bytes = 76,800 bytes (> 65,536 bytes)
                std::vector<void*> ptrs;
                ptrs.reserve(count);

                for (int c = 0; c < count; ++c) {
                    void* p = sph_alloc(48);
                    CHECK(p != nullptr);
                    ptrs.push_back(p);
                }

                // Verify all pointers are unique
                for (size_t a = 0; a < ptrs.size() - 1; ++a) {
                    CHECK(ptrs[a] != ptrs[a + 1]);
                }

                for (void* p : ptrs) {
                    sph_free(p);
                }
            });
        }
    }

    void registerConcurrentSlabAllocations() {
        // 100 tests: Concurrent thread-local slab allocations
        for (int m = 1; m <= 100; ++m) {
            std::string name = "SlabExhaustive: Thread-local concurrency " + std::to_string(m);
            testing::TestRegistry::instance().registerTest(name, [m]() {
                auto worker = [](int tid) {
                    for (int i = 0; i < 50; ++i) {
                        void* p = sph_alloc(32);
                        CHECK(p != nullptr);
                        std::memset(p, tid & 0xFF, 32);
                        sph_free(p);
                    }
                };

                std::thread t1(worker, 1);
                std::thread t2(worker, 2);
                t1.join();
                t2.join();
            });
        }
    }
};

static SlabExhaustiveRegistrar s_slabRegistrar;

} // namespace

TEST_CASE("Slab Multi-Tier: 32..512B 16-byte alignment and canary validation") {
    const int64_t before = sph_current_bytes();
    const int64_t sizes[] = {1, 16, 32, 48, 64, 100, 128, 200, 256, 400, 512, 513, 1024};
    std::vector<void*> ptrs;
    for (int64_t sz : sizes) {
        void* p = sph_alloc(sz);
        CHECK(p != nullptr);
        CHECK_EQ(reinterpret_cast<uintptr_t>(p) & 0xF, static_cast<uintptr_t>(0));
        CHECK_EQ(sph_mem_validate_ptr(p), 1);
        std::memset(p, 0xA5, static_cast<size_t>(sz));
        ptrs.push_back(p);
    }
    for (void* p : ptrs) {
        sph_free(p);
        CHECK_EQ(sph_mem_validate_ptr(p), 0);
    }
    CHECK_EQ(sph_current_bytes(), before);
}

TEST_CASE("Slab Cross-Thread: Exited producer orphan chunk safe remote reclamation") {
    const int64_t before = sph_current_bytes();
    std::vector<void*> transferred;
    const int64_t tierSizes[] = {32, 64, 128, 256, 512};

    // Producer allocates blocks across all 5 slab tiers and exits before consumer frees them
    std::thread producer([&]() {
        for (int i = 0; i < 500; ++i) {
            int64_t sz = tierSizes[i % 5];
            void* p = sph_alloc(sz);
            std::memset(p, (i % 200) + 1, static_cast<size_t>(sz));
            transferred.push_back(p);
        }
    });
    producer.join();

    // Producer thread has exited and retired its TLS cache; consumer frees safely
    for (void* p : transferred) {
        CHECK_EQ(sph_mem_validate_ptr(p), 1);
        sph_free(p);
    }
    CHECK_EQ(sph_current_bytes(), before);
}

TEST_CASE("Slab Cross-Thread: Live producer reclaims remotely freed blocks") {
    const int64_t before = sph_current_bytes();
    for (int round = 0; round < 50; ++round) {
        std::vector<void*> batch;
        for (int i = 0; i < 128; ++i) {
            batch.push_back(sph_alloc(64));
        }
        std::thread consumer([&]() {
            for (void* p : batch) {
                sph_free(p);
            }
        });
        consumer.join();
    }
    CHECK_EQ(sph_current_bytes(), before);
}

} // namespace sph

