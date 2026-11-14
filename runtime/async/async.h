#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Concurrency, Threading, and Atomics
// ============================================================================

typedef struct sph_atomic_i32 {
    int32_t value;
} sph_atomic_i32;

typedef struct sph_atomic_i64 {
    int64_t value;
} sph_atomic_i64;

typedef struct sph_atomic_bool {
    int32_t value;
} sph_atomic_bool;

// Memory ordering models
#define SPH_MEMORDER_RELAXED 0
#define SPH_MEMORDER_ACQUIRE 1
#define SPH_MEMORDER_RELEASE 2
#define SPH_MEMORDER_ACQ_REL 3
#define SPH_MEMORDER_SEQ_CST 4

// Atomics for i32
int32_t sph_atomic_load_i32(void* atomic_ptr, int32_t order);
void sph_atomic_store_i32(void* atomic_ptr, int32_t val, int32_t order);
int32_t sph_atomic_fetch_add_i32(void* atomic_ptr, int32_t val, int32_t order);
int32_t sph_atomic_fetch_sub_i32(void* atomic_ptr, int32_t val, int32_t order);
int32_t sph_atomic_exchange_i32(void* atomic_ptr, int32_t val, int32_t order);
int32_t sph_atomic_compare_exchange_i32(void* atomic_ptr, int32_t expected, int32_t desired, int32_t succ_order, int32_t fail_order);

// Atomics for i64
int64_t sph_atomic_load_i64(void* atomic_ptr, int32_t order);
void sph_atomic_store_i64(void* atomic_ptr, int64_t val, int32_t order);
int64_t sph_atomic_fetch_add_i64(void* atomic_ptr, int64_t val, int32_t order);
int64_t sph_atomic_fetch_sub_i64(void* atomic_ptr, int64_t val, int32_t order);
int64_t sph_atomic_exchange_i64(void* atomic_ptr, int64_t val, int32_t order);
int32_t sph_atomic_compare_exchange_i64(void* atomic_ptr, int64_t expected, int64_t desired, int32_t succ_order, int32_t fail_order);

// Atomics for bool
int32_t sph_atomic_load_bool(void* atomic_ptr, int32_t order);
void sph_atomic_store_bool(void* atomic_ptr, int32_t val, int32_t order);
int32_t sph_atomic_exchange_bool(void* atomic_ptr, int32_t val, int32_t order);
int32_t sph_atomic_compare_exchange_bool(void* atomic_ptr, int32_t expected, int32_t desired, int32_t succ_order, int32_t fail_order);

// Native Threading via Platform Abstraction Layer (PAL)
void* sph_thread_spawn(void* fn_ptr, void* arg);
void sph_thread_join(void* thread_handle);
void sph_thread_sleep_ms(int32_t ms);

// Coroutine, Promise & EventLoop Subsystem
typedef enum sph_promise_state {
    SPH_PROMISE_PENDING = 0,
    SPH_PROMISE_FULFILLED = 1,
    SPH_PROMISE_REJECTED = 2
} sph_promise_state;

typedef struct sph_promise {
    int32_t state;
    void* value;
    void* waiting_fiber;
} sph_promise;

void* sph_promise_create(void);
void sph_promise_resolve(void* promise_ptr, void* value);
void sph_promise_reject(void* promise_ptr, void* error);
int32_t sph_promise_is_ready(void* promise_ptr);
void* sph_promise_get_value(void* promise_ptr);
void* sph_promise_await(void* promise_ptr);

void sph_event_loop_init(void);
void* sph_event_loop_spawn(void* fn_ptr, void* arg);
void sph_event_loop_yield(void);
void sph_event_loop_run(void);

typedef int32_t (*sph_event_loop_pump_hook_fn)(void);
void sph_event_loop_set_pump_hook(sph_event_loop_pump_hook_fn hook);

#ifdef __cplusplus
}
#endif

