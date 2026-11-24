#include "common/test_framework.hpp"
#include "runtime/async/async.h"
#include "runtime/net/websocket.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

namespace sph {

TEST_CASE("Concurrency Stress: Multi-threaded atomic fetch-add contention") {
    int32_t atomicVal = 0;
    const int threadCount = 10;
    const int iterationsPerThread = 5000;

    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&atomicVal, iterationsPerThread]() {
            for (int i = 0; i < iterationsPerThread; ++i) {
                sph_atomic_fetch_add_i32(&atomicVal, 1, SPH_MEMORDER_SEQ_CST);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    int32_t finalVal = sph_atomic_load_i32(&atomicVal, SPH_MEMORDER_SEQ_CST);
    CHECK_EQ(finalVal, threadCount * iterationsPerThread);
}

TEST_CASE("Concurrency Stress: Multi-threaded atomic compare-exchange") {
    int64_t target = 0;
    const int threadCount = 8;
    const int attemptsPerThread = 1000;
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&target, &successCount, attemptsPerThread]() {
            for (int i = 0; i < attemptsPerThread; ++i) {
                int64_t current = sph_atomic_load_i64(&target, SPH_MEMORDER_RELAXED);
                if (sph_atomic_compare_exchange_i64(&target, current, current + 1, SPH_MEMORDER_SEQ_CST, SPH_MEMORDER_RELAXED)) {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    int64_t finalVal = sph_atomic_load_i64(&target, SPH_MEMORDER_SEQ_CST);
    CHECK_EQ(finalVal, static_cast<int64_t>(successCount.load()));
}

TEST_CASE("Concurrency Stress: Mutex protects shared critical section under contention") {
    void* mtx = sph_mutex_create();
    CHECK(mtx != nullptr);

    int sharedResource = 0;
    const int threadCount = 8;
    const int iterations = 5000;

    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([mtx, &sharedResource, iterations]() {
            for (int i = 0; i < iterations; ++i) {
                sph_mutex_lock(mtx);
                sharedResource++;
                sph_mutex_unlock(mtx);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    sph_mutex_destroy(mtx);
    CHECK_EQ(sharedResource, threadCount * iterations);
}

TEST_CASE("Concurrency Stress: Atomic Promise Resolution race condition immunity") {
    void* promise = sph_promise_create();
    CHECK(promise != nullptr);
    CHECK_EQ(sph_promise_is_ready(promise), 0);

    const int raceThreads = 10;
    std::vector<std::thread> racers;
    racers.reserve(raceThreads);

    int val1 = 100;
    int val2 = 200;

    for (int i = 0; i < raceThreads; ++i) {
        racers.emplace_back([promise, i, &val1, &val2]() {
            if (i % 2 == 0) {
                sph_promise_resolve(promise, &val1);
            } else {
                sph_promise_reject(promise, &val2);
            }
        });
    }

    for (auto& th : racers) {
        th.join();
    }

    // Exactly one state must have won; must be ready
    CHECK_EQ(sph_promise_is_ready(promise), 1);
    void* winnerVal = sph_promise_get_value(promise);
    CHECK(winnerVal == &val1 || winnerVal == &val2);

    sph_promise_destroy(promise);
}

TEST_CASE("Network & WebSocket: Handshake parsing and safe resource teardown") {
    const char* sampleRequest =
        "GET /chat HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";

    // Invalid fd should be rejected safely without crashing
    sph_ws_client* ws = sph_ws_accept_upgrade(-1, sampleRequest);
    CHECK(ws == nullptr);

    // Null safety tests
    CHECK_EQ(sph_ws_is_open(nullptr), 0);
    sph_ws_close(nullptr); // Must not crash
    CHECK_EQ(sph_ws_send_text(nullptr, "hello"), -1);
}

} // namespace sph

