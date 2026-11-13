#include "async.h"
#include "../pal.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#if !defined(_WIN32)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#if defined(__APPLE__)
#define _XOPEN_SOURCE 600
#endif
#include <ucontext.h>
#endif

// ============================================================================
// Concurrency, Threading, and Atomics (Phase 11 Milestone 11A)
// ============================================================================

static inline int sph_map_mem_order(int32_t order) {
    switch (order) {
        case SPH_MEMORDER_RELAXED: return __ATOMIC_RELAXED;
        case SPH_MEMORDER_ACQUIRE: return __ATOMIC_ACQUIRE;
        case SPH_MEMORDER_RELEASE: return __ATOMIC_RELEASE;
        case SPH_MEMORDER_ACQ_REL: return __ATOMIC_ACQ_REL;
        case SPH_MEMORDER_SEQ_CST:
        default:                   return __ATOMIC_SEQ_CST;
    }
}

// Atomics for i32
int32_t sph_atomic_load_i32(void* atomic_ptr, int32_t order) {
    if (!atomic_ptr) return 0;
    return __atomic_load_n((int32_t*)atomic_ptr, sph_map_mem_order(order));
}

void sph_atomic_store_i32(void* atomic_ptr, int32_t val, int32_t order) {
    if (!atomic_ptr) return;
    int ord = sph_map_mem_order(order);
    if (ord == __ATOMIC_ACQUIRE || ord == __ATOMIC_ACQ_REL) ord = __ATOMIC_RELAXED;
    __atomic_store_n((int32_t*)atomic_ptr, val, ord);
}

int32_t sph_atomic_fetch_add_i32(void* atomic_ptr, int32_t val, int32_t order) {
    if (!atomic_ptr) return 0;
    return __atomic_fetch_add((int32_t*)atomic_ptr, val, sph_map_mem_order(order));
}

int32_t sph_atomic_fetch_sub_i32(void* atomic_ptr, int32_t val, int32_t order) {
    if (!atomic_ptr) return 0;
    return __atomic_fetch_sub((int32_t*)atomic_ptr, val, sph_map_mem_order(order));
}

int32_t sph_atomic_exchange_i32(void* atomic_ptr, int32_t val, int32_t order) {
    if (!atomic_ptr) return 0;
    return __atomic_exchange_n((int32_t*)atomic_ptr, val, sph_map_mem_order(order));
}

int32_t sph_atomic_compare_exchange_i32(void* atomic_ptr, int32_t expected, int32_t desired, int32_t succ_order, int32_t fail_order) {
    if (!atomic_ptr) return 0;
    int32_t exp = expected;
    int s_ord = sph_map_mem_order(succ_order);
    int f_ord = sph_map_mem_order(fail_order);
    if (f_ord > s_ord) f_ord = s_ord;
    if (f_ord == __ATOMIC_RELEASE) f_ord = __ATOMIC_RELAXED;
    if (f_ord == __ATOMIC_ACQ_REL) f_ord = __ATOMIC_ACQUIRE;
    bool success = __atomic_compare_exchange_n((int32_t*)atomic_ptr, &exp, desired, 0, s_ord, f_ord);
    return success ? 1 : 0;
}

// Atomics for i64
int64_t sph_atomic_load_i64(void* atomic_ptr, int32_t order) {
    if (!atomic_ptr) return 0;
    return __atomic_load_n((int64_t*)atomic_ptr, sph_map_mem_order(order));
}

void sph_atomic_store_i64(void* atomic_ptr, int64_t val, int32_t order) {
    if (!atomic_ptr) return;
    int ord = sph_map_mem_order(order);
    if (ord == __ATOMIC_ACQUIRE || ord == __ATOMIC_ACQ_REL) ord = __ATOMIC_RELAXED;
    __atomic_store_n((int64_t*)atomic_ptr, val, ord);
}

int64_t sph_atomic_fetch_add_i64(void* atomic_ptr, int64_t val, int32_t order) {
    if (!atomic_ptr) return 0;
    return __atomic_fetch_add((int64_t*)atomic_ptr, val, sph_map_mem_order(order));
}

int64_t sph_atomic_fetch_sub_i64(void* atomic_ptr, int64_t val, int32_t order) {
    if (!atomic_ptr) return 0;
    return __atomic_fetch_sub((int64_t*)atomic_ptr, val, sph_map_mem_order(order));
}

int64_t sph_atomic_exchange_i64(void* atomic_ptr, int64_t val, int32_t order) {
    if (!atomic_ptr) return 0;
    return __atomic_exchange_n((int64_t*)atomic_ptr, val, sph_map_mem_order(order));
}

int32_t sph_atomic_compare_exchange_i64(void* atomic_ptr, int64_t expected, int64_t desired, int32_t succ_order, int32_t fail_order) {
    if (!atomic_ptr) return 0;
    int64_t exp = expected;
    int s_ord = sph_map_mem_order(succ_order);
    int f_ord = sph_map_mem_order(fail_order);
    if (f_ord > s_ord) f_ord = s_ord;
    if (f_ord == __ATOMIC_RELEASE) f_ord = __ATOMIC_RELAXED;
    if (f_ord == __ATOMIC_ACQ_REL) f_ord = __ATOMIC_ACQUIRE;
    bool success = __atomic_compare_exchange_n((int64_t*)atomic_ptr, &exp, desired, 0, s_ord, f_ord);
    return success ? 1 : 0;
}

// Atomics for bool
int32_t sph_atomic_load_bool(void* atomic_ptr, int32_t order) {
    if (!atomic_ptr) return 0;
    return __atomic_load_n((int32_t*)atomic_ptr, sph_map_mem_order(order)) ? 1 : 0;
}

void sph_atomic_store_bool(void* atomic_ptr, int32_t val, int32_t order) {
    if (!atomic_ptr) return;
    int ord = sph_map_mem_order(order);
    if (ord == __ATOMIC_ACQUIRE || ord == __ATOMIC_ACQ_REL) ord = __ATOMIC_RELAXED;
    __atomic_store_n((int32_t*)atomic_ptr, val ? 1 : 0, ord);
}

int32_t sph_atomic_exchange_bool(void* atomic_ptr, int32_t val, int32_t order) {
    if (!atomic_ptr) return 0;
    return __atomic_exchange_n((int32_t*)atomic_ptr, val ? 1 : 0, sph_map_mem_order(order));
}

int32_t sph_atomic_compare_exchange_bool(void* atomic_ptr, int32_t expected, int32_t desired, int32_t succ_order, int32_t fail_order) {
    if (!atomic_ptr) return 0;
    int32_t exp = expected ? 1 : 0;
    int s_ord = sph_map_mem_order(succ_order);
    int f_ord = sph_map_mem_order(fail_order);
    if (f_ord > s_ord) f_ord = s_ord;
    if (f_ord == __ATOMIC_RELEASE) f_ord = __ATOMIC_RELAXED;
    if (f_ord == __ATOMIC_ACQ_REL) f_ord = __ATOMIC_ACQUIRE;
    bool success = __atomic_compare_exchange_n((int32_t*)atomic_ptr, &exp, desired ? 1 : 0, 0, s_ord, f_ord);
    return success ? 1 : 0;
}

// ============================================================================
// Native Threading via Platform Abstraction Layer (PAL)
// ============================================================================

typedef struct sph_thread_start_info {
    void* fn_ptr;
    void* arg;
} sph_thread_start_info;

#if defined(_WIN32)

static DWORD WINAPI sph_thread_trampoline(LPVOID lpParam) {
    sph_thread_start_info* info = (sph_thread_start_info*)lpParam;
    void* fn_ptr = info->fn_ptr;
    void* arg = info->arg;
    free(info);

    typedef void* (*sph_thread_fn)(void*);
    sph_thread_fn func = (sph_thread_fn)(intptr_t)fn_ptr;
    func(arg);
    return 0;
}

void* sph_thread_spawn(void* fn_ptr, void* arg) {
    if (!fn_ptr) return NULL;
    sph_thread_start_info* info = (sph_thread_start_info*)malloc(sizeof(sph_thread_start_info));
    if (!info) return NULL;
    info->fn_ptr = fn_ptr;
    info->arg = arg;
    HANDLE hThread = CreateThread(NULL, 0, sph_thread_trampoline, info, 0, NULL);
    return (void*)hThread;
}

void sph_thread_join(void* thread_handle) {
    if (!thread_handle) return;
    HANDLE hThread = (HANDLE)thread_handle;
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
}

void sph_thread_sleep_ms(int32_t ms) {
    if (ms > 0) {
        Sleep((DWORD)ms);
    }
}

#else

static void* sph_posix_thread_trampoline(void* param) {
    sph_thread_start_info* info = (sph_thread_start_info*)param;
    void* fn_ptr = info->fn_ptr;
    void* arg = info->arg;
    free(info);

    typedef void* (*sph_thread_fn)(void*);
    sph_thread_fn func = (sph_thread_fn)(intptr_t)fn_ptr;
    func(arg);
    return NULL;
}

void* sph_thread_spawn(void* fn_ptr, void* arg) {
    if (!fn_ptr) return NULL;
    sph_thread_start_info* info = (sph_thread_start_info*)malloc(sizeof(sph_thread_start_info));
    if (!info) return NULL;
    info->fn_ptr = fn_ptr;
    info->arg = arg;

    pthread_t* tid = (pthread_t*)malloc(sizeof(pthread_t));
    if (!tid) {
        free(info);
        return NULL;
    }
    if (pthread_create(tid, NULL, sph_posix_thread_trampoline, info) != 0) {
        free(info);
        free(tid);
        return NULL;
    }
    return (void*)tid;
}

void sph_thread_join(void* thread_handle) {
    if (!thread_handle) return;
    pthread_t* tid = (pthread_t*)thread_handle;
    pthread_join(*tid, NULL);
    free(tid);
}

void sph_thread_sleep_ms(int32_t ms) {
    if (ms > 0) {
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }
}

#endif

// ============================================================================
// Coroutine, Promise & EventLoop Subsystem
// ============================================================================

typedef struct sph_coroutine_node {
    void* fiber;
    struct sph_coroutine_node* next;
} sph_coroutine_node;

typedef struct sph_event_loop_state {
    void* main_fiber;
    void* running_fiber;
    sph_coroutine_node* queue_head;
    sph_coroutine_node* queue_tail;
    int is_initialized;
} sph_event_loop_state;

static sph_event_loop_state g_event_loop = {0};

static void sph_event_loop_queue_fiber(void* fiber) {
    if (!fiber) return;
    sph_coroutine_node* node = (sph_coroutine_node*)malloc(sizeof(sph_coroutine_node));
    if (!node) return;
    node->fiber = fiber;
    node->next = NULL;
    if (!g_event_loop.queue_head) {
        g_event_loop.queue_head = node;
        g_event_loop.queue_tail = node;
    } else {
        g_event_loop.queue_tail->next = node;
        g_event_loop.queue_tail = node;
    }
}

static void* sph_event_loop_pop_fiber(void) {
    if (!g_event_loop.queue_head) return NULL;
    sph_coroutine_node* node = g_event_loop.queue_head;
    void* fiber = node->fiber;
    g_event_loop.queue_head = node->next;
    if (!g_event_loop.queue_head) {
        g_event_loop.queue_tail = NULL;
    }
    free(node);
    return fiber;
}

void* sph_promise_create(void) {
    sph_promise* p = (sph_promise*)malloc(sizeof(sph_promise));
    if (!p) return NULL;
    p->state = SPH_PROMISE_PENDING;
    p->value = NULL;
    p->waiting_fiber = NULL;
    return (void*)p;
}

void sph_promise_resolve(void* promise_ptr, void* value) {
    if (!promise_ptr) return;
    sph_promise* p = (sph_promise*)promise_ptr;
    if (p->state != SPH_PROMISE_PENDING) return;
    p->state = SPH_PROMISE_FULFILLED;
    p->value = value;
    if (p->waiting_fiber) {
        sph_event_loop_queue_fiber(p->waiting_fiber);
        p->waiting_fiber = NULL;
    }
}

void sph_promise_reject(void* promise_ptr, void* error) {
    if (!promise_ptr) return;
    sph_promise* p = (sph_promise*)promise_ptr;
    if (p->state != SPH_PROMISE_PENDING) return;
    p->state = SPH_PROMISE_REJECTED;
    p->value = error;
    if (p->waiting_fiber) {
        sph_event_loop_queue_fiber(p->waiting_fiber);
        p->waiting_fiber = NULL;
    }
}

int32_t sph_promise_is_ready(void* promise_ptr) {
    if (!promise_ptr) return 1;
    sph_promise* p = (sph_promise*)promise_ptr;
    return p->state != SPH_PROMISE_PENDING ? 1 : 0;
}

void* sph_promise_get_value(void* promise_ptr) {
    if (!promise_ptr) return NULL;
    sph_promise* p = (sph_promise*)promise_ptr;
    return p->value;
}

#if defined(_WIN32)

void sph_event_loop_init(void) {
    if (g_event_loop.is_initialized) return;
    g_event_loop.main_fiber = ConvertThreadToFiber(NULL);
    if (!g_event_loop.main_fiber) {
        g_event_loop.main_fiber = GetCurrentFiber();
    }
    g_event_loop.running_fiber = g_event_loop.main_fiber;
    g_event_loop.is_initialized = 1;
}

typedef struct sph_coroutine_start_info {
    void* fn_ptr;
    void* arg;
    void* promise;
    void* fiber;
} sph_coroutine_start_info;

static void* g_fiber_to_delete = NULL;

static VOID WINAPI sph_coroutine_trampoline(LPVOID lpParam) {
    sph_coroutine_start_info* info = (sph_coroutine_start_info*)lpParam;
    void* fn_ptr = info->fn_ptr;
    void* arg = info->arg;
    void* promise = info->promise;
    void* self_fiber = info->fiber;
    free(info);

    typedef void* (*sph_coro_fn)(void*);
    sph_coro_fn func = (sph_coro_fn)(intptr_t)fn_ptr;
    void* retVal = func(arg);

    if (promise) {
        sph_promise_resolve(promise, retVal);
    }

    g_fiber_to_delete = self_fiber;
    SwitchToFiber(g_event_loop.main_fiber);
}

void* sph_event_loop_spawn(void* fn_ptr, void* arg) {
    if (!fn_ptr) return NULL;
    sph_event_loop_init();

    void* promise = sph_promise_create();
    sph_coroutine_start_info* info = (sph_coroutine_start_info*)malloc(sizeof(sph_coroutine_start_info));
    if (!info) return NULL;
    info->fn_ptr = fn_ptr;
    info->arg = arg;
    info->promise = promise;

    void* fiber = CreateFiber(64 * 1024, sph_coroutine_trampoline, info);
    if (!fiber) {
        free(info);
        return NULL;
    }
    info->fiber = fiber;

    sph_event_loop_queue_fiber(fiber);
    return promise;
}

void sph_event_loop_yield(void) {
    sph_event_loop_init();
    void* cur = GetCurrentFiber();
    if (cur && cur != g_event_loop.main_fiber) {
        sph_event_loop_queue_fiber(cur);
        g_event_loop.running_fiber = g_event_loop.main_fiber;
        SwitchToFiber(g_event_loop.main_fiber);
    }
}

void* sph_promise_await(void* promise_ptr) {
    if (!promise_ptr) return NULL;
    sph_promise* p = (sph_promise*)promise_ptr;
    if (p->state != SPH_PROMISE_PENDING) {
        return p->value;
    }

    sph_event_loop_init();
    void* cur = GetCurrentFiber();

    if (cur && cur != g_event_loop.main_fiber) {
        while (p->state == SPH_PROMISE_PENDING) {
            p->waiting_fiber = cur;
            g_event_loop.running_fiber = g_event_loop.main_fiber;
            SwitchToFiber(g_event_loop.main_fiber);
        }
        return p->value;
    } else {
        while (p->state == SPH_PROMISE_PENDING) {
            void* next_fiber = sph_event_loop_pop_fiber();
            if (!next_fiber) {
                break;
            }
            g_event_loop.running_fiber = next_fiber;
            SwitchToFiber(next_fiber);
            if (g_fiber_to_delete) {
                DeleteFiber(g_fiber_to_delete);
                g_fiber_to_delete = NULL;
            }
            g_event_loop.running_fiber = g_event_loop.main_fiber;
        }
        return p->value;
    }
}

void sph_event_loop_run(void) {
    sph_event_loop_init();
    while (1) {
        void* next_fiber = sph_event_loop_pop_fiber();
        if (!next_fiber) {
            break;
        }
        g_event_loop.running_fiber = next_fiber;
        SwitchToFiber(next_fiber);
        if (g_fiber_to_delete) {
            DeleteFiber(g_fiber_to_delete);
            g_fiber_to_delete = NULL;
        }
        g_event_loop.running_fiber = g_event_loop.main_fiber;
    }
}

#else

// POSIX Coroutine Fiber implementation via ucontext_t
typedef struct sph_posix_coro {
    ucontext_t ctx;
    char stack[64 * 1024];
    void* fn_ptr;
    void* arg;
    void* promise;
    int is_finished;
} sph_posix_coro;

static ucontext_t g_posix_main_ctx;
static sph_posix_coro* g_current_coro = NULL;

void sph_event_loop_init(void) {
    if (g_event_loop.is_initialized) return;
    getcontext(&g_posix_main_ctx);
    g_event_loop.main_fiber = (void*)&g_posix_main_ctx;
    g_event_loop.running_fiber = g_event_loop.main_fiber;
    g_event_loop.is_initialized = 1;
}

static void sph_posix_coro_entry(void) {
    sph_posix_coro* coro = g_current_coro;
    if (coro) {
        typedef void* (*sph_coro_fn)(void*);
        sph_coro_fn func = (sph_coro_fn)(intptr_t)coro->fn_ptr;
        void* retVal = func(coro->arg);
        if (coro->promise) {
            sph_promise_resolve(coro->promise, retVal);
        }
        coro->is_finished = 1;
        setcontext(&g_posix_main_ctx);
    }
}

void* sph_event_loop_spawn(void* fn_ptr, void* arg) {
    if (!fn_ptr) return NULL;
    sph_event_loop_init();

    void* volatile promise = sph_promise_create();
    sph_posix_coro* volatile coro = (sph_posix_coro*)malloc(sizeof(sph_posix_coro));
    if (!coro) return NULL;
    coro->fn_ptr = fn_ptr;
    coro->arg = arg;
    coro->promise = promise;
    coro->is_finished = 0;

    getcontext(&coro->ctx);
    coro->ctx.uc_stack.ss_sp = coro->stack;
    coro->ctx.uc_stack.ss_size = sizeof(coro->stack);
    coro->ctx.uc_link = &g_posix_main_ctx;
    makecontext(&coro->ctx, sph_posix_coro_entry, 0);

    sph_event_loop_queue_fiber(coro);
    return promise;
}

void sph_event_loop_yield(void) {
    sph_event_loop_init();
    if (g_current_coro) {
        sph_posix_coro* cur = g_current_coro;
        sph_event_loop_queue_fiber(cur);
        g_current_coro = NULL;
        swapcontext(&cur->ctx, &g_posix_main_ctx);
    }
}

void* sph_promise_await(void* promise_ptr) {
    if (!promise_ptr) return NULL;
    sph_promise* p = (sph_promise*)promise_ptr;
    if (p->state != SPH_PROMISE_PENDING) {
        return p->value;
    }

    sph_event_loop_init();
    while (p->state == SPH_PROMISE_PENDING) {
        void* next = sph_event_loop_pop_fiber();
        if (!next) break;
        sph_posix_coro* coro = (sph_posix_coro*)next;
        g_current_coro = coro;
        swapcontext(&g_posix_main_ctx, &coro->ctx);
        if (coro->is_finished) {
            free(coro);
        }
    }
    return p->value;
}

void sph_event_loop_run(void) {
    sph_event_loop_init();
    while (1) {
        void* next = sph_event_loop_pop_fiber();
        if (!next) break;
        sph_posix_coro* coro = (sph_posix_coro*)next;
        g_current_coro = coro;
        swapcontext(&g_posix_main_ctx, &coro->ctx);
        if (coro->is_finished) {
            free(coro);
        }
    }
}

#endif

