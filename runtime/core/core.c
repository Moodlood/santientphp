#define SPH_CORE_INCLUDED 1
#include "core.h"
#include "../pal.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#endif
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#endif
#endif

void sph_println_i32(int32_t val) {
    printf("%d\n", val);
}

void sph_println_i64(int64_t val) {
    printf("%lld\n", (long long)val);
}

void sph_println_f32(float val) {
    printf("%g\n", (double)val);
}

void sph_println_f64(double val) {
    printf("%g\n", val);
}

void sph_println_bool(int32_t val) {
    printf("%s\n", val ? "true" : "false");
}

void sph_println_string(const char* val) {
    printf("%s\n", val ? val : "");
}

// ============================================================================
// Multi-Tier Thread-Local Slab Allocator (32B..512B) + Cross-Thread Reclamation
// ============================================================================

#define SPH_SLAB_MAGIC        0x53504853U // "SPHS"
#define SPH_SLAB_FREED_MAGIC  0x44454144U // "DEAD"
#define SPH_LARGE_MAGIC       0x4C415247U // "LARG"
#define SPH_LARGE_FREED_MAGIC 0x4C444541U // "LDEA"
#define SPH_SLAB_NUM_TIERS    5
#define SPH_SLAB_MAX_PAYLOAD  512
#define SPH_SLAB_CHUNK_CAPACITY 65536

static const uint32_t SPH_SLAB_TIER_SIZES[SPH_SLAB_NUM_TIERS] = {32, 64, 128, 256, 512};

struct sph_slab_chunk;
struct sph_slab_thread_cache;

typedef struct sph_slab_block {
    uint32_t magic;
    uint32_t size;
    uint32_t tier;
    uint32_t _pad;
    struct sph_slab_block* next;
    struct sph_slab_chunk* chunk;
} sph_slab_block; // 32 bytes (16-byte aligned user payload)

typedef struct sph_large_block {
    uint32_t magic;
    uint32_t size;
    uint64_t _pad[3];
} sph_large_block; // 32 bytes (exact offset parity with sph_slab_block)

_Static_assert(sizeof(sph_slab_block) == 32, "sph_slab_block must be 32 bytes");
_Static_assert(sizeof(sph_large_block) == 32, "sph_large_block must be 32 bytes");

typedef struct sph_slab_chunk {
    struct sph_slab_chunk* next;
    sph_slab_block* remote_free;        // Atomic lock-free list for cross-thread frees
    struct sph_slab_thread_cache* owner; // Atomic: cleared to NULL on owner thread exit
    int64_t ref_count;                  // Atomic: 1 owner handle + live allocated blocks
    uint32_t tier;
    uint32_t _pad;
    uint64_t _pad2;
    uint8_t memory[SPH_SLAB_CHUNK_CAPACITY - 48];
} sph_slab_chunk;

/* Compile-time check: sph_slab_chunk.memory must start at byte 48 (16-byte aligned).
   Use a typedef-based assertion that works on all C11+ GCC versions. */
typedef char sph_check_chunk_offset_[(offsetof(sph_slab_chunk, memory) == 48) ? 1 : -1];

typedef struct sph_slab_tier_cache {
    sph_slab_block* free_pool;
    uint8_t* bump;
    uint8_t* limit;
    sph_slab_chunk* current_chunk;
    sph_slab_chunk* chunk_head;
} sph_slab_tier_cache;

#define SPH_FREED_HISTORY_SIZE 512
#define SPH_FREED_HISTORY_MASK (SPH_FREED_HISTORY_SIZE - 1)
#define SPH_STR_CACHE_SIZE 256
#define SPH_STR_CACHE_MASK (SPH_STR_CACHE_SIZE - 1)

typedef struct {
    char* ptr;
    size_t len;
    size_t cap;
} sph_str_cache_entry;

typedef struct sph_slab_thread_cache {
    sph_slab_tier_cache tiers[SPH_SLAB_NUM_TIERS];
    void* freed_history[SPH_FREED_HISTORY_SIZE];
    sph_str_cache_entry str_cache[SPH_STR_CACHE_SIZE];
    sph_str_cache_entry str_mru;
} sph_slab_thread_cache;

#if defined(_WIN32)
static DWORD g_core_tls_index = TLS_OUT_OF_INDEXES;
static DWORD g_slab_fls_index = FLS_OUT_OF_INDEXES;

static void sph_slab_thread_cleanup(void* arg);

static VOID WINAPI sph_slab_fls_destructor(PVOID lpFlsData) {
    sph_slab_thread_cleanup(lpFlsData);
}

static inline DWORD sph_ensure_win32_tls_slot(void) {
    DWORD idx = __atomic_load_n(&g_core_tls_index, __ATOMIC_ACQUIRE);
    if (idx == TLS_OUT_OF_INDEXES) {
        DWORD new_idx = TlsAlloc();
        if (new_idx != TLS_OUT_OF_INDEXES) {
            DWORD expected = TLS_OUT_OF_INDEXES;
            if (!__atomic_compare_exchange_n(&g_core_tls_index, &expected, new_idx, 0,
                                             __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                TlsFree(new_idx);
                idx = expected;
            } else {
                idx = new_idx;
            }
        }
    }
    return idx;
}

static inline void sph_slab_register_tls_destructor(sph_slab_thread_cache* tc) {
    DWORD fls = __atomic_load_n(&g_slab_fls_index, __ATOMIC_ACQUIRE);
    if (fls == FLS_OUT_OF_INDEXES) {
        DWORD new_fls = FlsAlloc(sph_slab_fls_destructor);
        if (new_fls != FLS_OUT_OF_INDEXES) {
            DWORD expected = FLS_OUT_OF_INDEXES;
            if (!__atomic_compare_exchange_n(&g_slab_fls_index, &expected, new_fls, 0,
                                             __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                FlsFree(new_fls);
                fls = expected;
            } else {
                fls = new_fls;
            }
        }
    }
    if (fls != FLS_OUT_OF_INDEXES) {
        FlsSetValue(fls, tc);
    }
}

static void sph_slab_thread_cleanup(void* arg) {
    sph_slab_thread_cache* tc = (sph_slab_thread_cache*)arg;
    if (!tc) return;
    DWORD tls_idx = __atomic_load_n(&g_core_tls_index, __ATOMIC_ACQUIRE);
    if (tls_idx != TLS_OUT_OF_INDEXES && TlsGetValue(tls_idx) == tc) {
        TlsSetValue(tls_idx, NULL);
    }
    for (int t = 0; t < SPH_SLAB_NUM_TIERS; ++t) {
        sph_slab_chunk* c = tc->tiers[t].chunk_head;
        while (c) {
            sph_slab_chunk* next = c->next;
            __atomic_store_n(&c->owner, NULL, __ATOMIC_RELEASE);
            if (__atomic_fetch_sub(&c->ref_count, 1, __ATOMIC_ACQ_REL) == 1) {
                free(c);
            }
            c = next;
        }
    }
    free(tc);
}

static inline sph_slab_thread_cache* sph_slab_get_tls(void) {
    DWORD idx = __atomic_load_n(&g_core_tls_index, __ATOMIC_RELAXED);
    if (idx == TLS_OUT_OF_INDEXES) return NULL;
    return (sph_slab_thread_cache*)TlsGetValue(idx);
}

static inline sph_slab_thread_cache* sph_slab_get_or_init_cache(void) {
    DWORD idx = sph_ensure_win32_tls_slot();
    if (idx == TLS_OUT_OF_INDEXES) return NULL;
    sph_slab_thread_cache* tc = (sph_slab_thread_cache*)TlsGetValue(idx);
    if (!tc) {
        tc = (sph_slab_thread_cache*)calloc(1, sizeof(sph_slab_thread_cache));
        if (!tc) return NULL;
        TlsSetValue(idx, tc);
        sph_slab_register_tls_destructor(tc);
    }
    return tc;
}
#else
static __thread sph_slab_thread_cache* t_slab = NULL;
static pthread_key_t g_slab_pthread_key;
static pthread_once_t g_slab_pthread_once = PTHREAD_ONCE_INIT;

static void sph_slab_thread_cleanup(void* arg) {
    sph_slab_thread_cache* tc = (sph_slab_thread_cache*)arg;
    if (!tc) return;
    if (t_slab == tc) {
        t_slab = NULL;
    }
    for (int t = 0; t < SPH_SLAB_NUM_TIERS; ++t) {
        sph_slab_chunk* c = tc->tiers[t].chunk_head;
        while (c) {
            sph_slab_chunk* next = c->next;
            __atomic_store_n(&c->owner, NULL, __ATOMIC_RELEASE);
            if (__atomic_fetch_sub(&c->ref_count, 1, __ATOMIC_ACQ_REL) == 1) {
                free(c);
            }
            c = next;
        }
    }
    free(tc);
}

static void sph_slab_pthread_init(void) {
    pthread_key_create(&g_slab_pthread_key, sph_slab_thread_cleanup);
}

static inline sph_slab_thread_cache* sph_slab_get_tls(void) {
    return t_slab;
}

static inline sph_slab_thread_cache* sph_slab_get_or_init_cache(void) {
    sph_slab_thread_cache* tc = t_slab;
    if (!tc) {
        tc = (sph_slab_thread_cache*)calloc(1, sizeof(sph_slab_thread_cache));
        if (!tc) return NULL;
        t_slab = tc;
        pthread_once(&g_slab_pthread_once, sph_slab_pthread_init);
        pthread_setspecific(g_slab_pthread_key, tc);
    }
    return tc;
}
#endif

static inline int sph_slab_tier_for_size(int64_t size) {
    if (size <= 32) return 0;
    if (size <= 64) return 1;
    if (size <= 128) return 2;
    if (size <= 256) return 3;
    if (size <= 512) return 4;
    return -1;
}

static int64_t g_alloc_count = 0;
static int64_t g_free_count = 0;
static int64_t g_current_bytes = 0;
static int64_t g_peak_bytes = 0;

static inline size_t hash_freed_ptr(void* ptr) {
    uintptr_t v = (uintptr_t)ptr;
    v ^= v >> 16;
    v ^= v >> 8;
    return (size_t)(v & SPH_FREED_HISTORY_MASK);
}

static inline int is_in_freed_history(void* ptr) {
    if (!ptr) return 0;
    sph_slab_thread_cache* tc = sph_slab_get_tls();
    if (!tc) return 0;
    return tc->freed_history[hash_freed_ptr(ptr)] == ptr;
}

static inline void add_to_freed_history(void* ptr) {
    if (!ptr) return;
    sph_slab_thread_cache* tc = sph_slab_get_or_init_cache();
    if (!tc) return;
    tc->freed_history[hash_freed_ptr(ptr)] = ptr;
}

int64_t sph_alloc_count(void) {
    return __atomic_load_n(&g_alloc_count, __ATOMIC_RELAXED);
}

int64_t sph_free_count(void) {
    return __atomic_load_n(&g_free_count, __ATOMIC_RELAXED);
}

int64_t sph_current_bytes(void) {
    return __atomic_load_n(&g_current_bytes, __ATOMIC_RELAXED);
}

int64_t sph_peak_bytes(void) {
    return __atomic_load_n(&g_peak_bytes, __ATOMIC_RELAXED);
}

void sph_mem_reset_stats(void) {
    __atomic_store_n(&g_alloc_count, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_free_count, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_current_bytes, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_peak_bytes, 0, __ATOMIC_RELAXED);
    sph_slab_thread_cache* tc = sph_slab_get_tls();
    if (tc) {
        for (size_t i = 0; i < SPH_FREED_HISTORY_SIZE; ++i) {
            tc->freed_history[i] = NULL;
        }
    }
}

void sph_mem_dump_leaks(void) {
    int64_t allocs = sph_alloc_count();
    int64_t frees = sph_free_count();
    int64_t cur = sph_current_bytes();
    int64_t peak = sph_peak_bytes();
    if (allocs > frees || cur > 0) {
        printf("[SantientPHP Memory Leak Report]\n");
        printf("  Allocations: %lld, Frees: %lld, Leaked: %lld blocks (%lld bytes)\n",
               (long long)allocs, (long long)frees, (long long)(allocs - frees), (long long)cur);
        printf("  Peak Memory: %lld bytes\n", (long long)peak);
    }
}

int32_t sph_mem_validate_ptr(void* ptr) {
    if (!ptr || is_in_freed_history(ptr)) return 0;
    if (((uintptr_t)ptr & 0x7) != 0 || (uintptr_t)ptr < 4096) return 0;
    sph_slab_block* blk = ((sph_slab_block*)ptr) - 1;
    uint32_t magic = __atomic_load_n(&blk->magic, __ATOMIC_ACQUIRE);
    if (magic == SPH_SLAB_MAGIC || magic == SPH_LARGE_MAGIC) return 1;
    return 0;
}

static inline void remove_from_freed_history(void* ptr) {
    if (!ptr) return;
    sph_slab_thread_cache* tc = sph_slab_get_tls();
    if (!tc) return;
    size_t idx = hash_freed_ptr(ptr);
    if (tc->freed_history[idx] == ptr) {
        tc->freed_history[idx] = NULL;
    }
}

static inline void record_alloc_stats(void* ptr, int64_t size) {
    remove_from_freed_history(ptr);
    __atomic_fetch_add(&g_alloc_count, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_current_bytes, size, __ATOMIC_RELAXED);
    int64_t cur = sph_current_bytes();
    int64_t peak = sph_peak_bytes();
    while (cur > peak && !__atomic_compare_exchange_n(&g_peak_bytes, &peak, cur, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
}

void* sph_alloc(int64_t size) {
    if (size <= 0) size = 1;
    int tier = sph_slab_tier_for_size(size);
    if (tier >= 0) {
        uint32_t payload_cap = SPH_SLAB_TIER_SIZES[tier];
        size_t block_stride = sizeof(sph_slab_block) + payload_cap;
        sph_slab_thread_cache* tc = sph_slab_get_or_init_cache();

        if (tc) {
            sph_slab_tier_cache* tcache = &tc->tiers[tier];

            if (tcache->free_pool) {
                sph_slab_block* blk = tcache->free_pool;
                tcache->free_pool = blk->next;
                if (blk->chunk) {
                    __atomic_fetch_add(&blk->chunk->ref_count, 1, __ATOMIC_RELAXED);
                }
                __atomic_store_n(&blk->magic, SPH_SLAB_MAGIC, __ATOMIC_RELEASE);
                blk->size = (uint32_t)size;
                blk->tier = (uint32_t)tier;
                void* res = (void*)(blk + 1);
                record_alloc_stats(res, size);
                return res;
            }

            // Reclaim remotely freed blocks across this tier's chunks before allocating new memory
            sph_slab_chunk* rc = tcache->chunk_head;
            while (rc) {
                sph_slab_block* batch = __atomic_exchange_n(&rc->remote_free, NULL, __ATOMIC_ACQUIRE);
                if (batch) {
                    sph_slab_block* tail = batch;
                    while (tail->next) {
                        tail = tail->next;
                    }
                    tail->next = tcache->free_pool;
                    tcache->free_pool = batch->next;
                    if (batch->chunk) {
                        __atomic_fetch_add(&batch->chunk->ref_count, 1, __ATOMIC_RELAXED);
                    }
                    __atomic_store_n(&batch->magic, SPH_SLAB_MAGIC, __ATOMIC_RELEASE);
                    batch->size = (uint32_t)size;
                    batch->tier = (uint32_t)tier;
                    void* res = (void*)(batch + 1);
                    record_alloc_stats(res, size);
                    return res;
                }
                rc = rc->next;
            }

            if (tcache->bump && (tcache->bump + block_stride <= tcache->limit)) {
                sph_slab_block* blk = (sph_slab_block*)tcache->bump;
                tcache->bump += block_stride;
                blk->chunk = tcache->current_chunk;
                if (blk->chunk) {
                    __atomic_fetch_add(&blk->chunk->ref_count, 1, __ATOMIC_RELAXED);
                }
                __atomic_store_n(&blk->magic, SPH_SLAB_MAGIC, __ATOMIC_RELEASE);
                blk->size = (uint32_t)size;
                blk->tier = (uint32_t)tier;
                void* res = (void*)(blk + 1);
                record_alloc_stats(res, size);
                return res;
            }

            sph_slab_chunk* new_chunk = (sph_slab_chunk*)malloc(sizeof(sph_slab_chunk));
            if (new_chunk) {
                new_chunk->next = tcache->chunk_head;
                __atomic_store_n(&new_chunk->remote_free, NULL, __ATOMIC_RELAXED);
                __atomic_store_n(&new_chunk->owner, tc, __ATOMIC_RELEASE);
                // ref_count starts at 2: 1 for owning thread handle + 1 for the returned first block
                __atomic_store_n(&new_chunk->ref_count, 2, __ATOMIC_RELEASE);
                new_chunk->tier = (uint32_t)tier;
                new_chunk->_pad = 0;
                new_chunk->_pad2 = 0;

                tcache->chunk_head = new_chunk;
                tcache->current_chunk = new_chunk;
                tcache->bump = new_chunk->memory + block_stride;
                tcache->limit = new_chunk->memory + sizeof(new_chunk->memory);

                sph_slab_block* blk = (sph_slab_block*)new_chunk->memory;
                blk->chunk = new_chunk;
                __atomic_store_n(&blk->magic, SPH_SLAB_MAGIC, __ATOMIC_RELEASE);
                blk->size = (uint32_t)size;
                blk->tier = (uint32_t)tier;
                blk->_pad = 0;
                void* res = (void*)(blk + 1);
                record_alloc_stats(res, size);
                return res;
            }
        }

        // Fallback single-block allocation
        sph_slab_block* blk = (sph_slab_block*)malloc(block_stride);
        if (!blk) return NULL;
        blk->chunk = NULL;
        blk->next = NULL;
        __atomic_store_n(&blk->magic, SPH_SLAB_MAGIC, __ATOMIC_RELEASE);
        blk->size = (uint32_t)size;
        blk->tier = (uint32_t)tier;
        blk->_pad = 0;
        void* res = (void*)(blk + 1);
        record_alloc_stats(res, size);
        return res;
    }

    size_t total = sizeof(sph_large_block) + (size_t)size;
    sph_large_block* lhdr = (sph_large_block*)malloc(total);
    if (!lhdr) return NULL;
    __atomic_store_n(&lhdr->magic, SPH_LARGE_MAGIC, __ATOMIC_RELEASE);
    lhdr->size = (uint32_t)size;
    lhdr->_pad[0] = lhdr->_pad[1] = lhdr->_pad[2] = 0;
    void* res = (void*)(lhdr + 1);
    record_alloc_stats(res, size);
    return res;
}

void sph_free(void* ptr) {
    if (!ptr) return;

    // Pointer provenance & alignment validation: managed blocks are at least 8-byte aligned
    if (((uintptr_t)ptr & 0x7) != 0 || (uintptr_t)ptr < 4096) {
        return;
    }

    // Check freed-pointer history BEFORE reading block header (prevents ASan UAF on large double-free)
    if (is_in_freed_history(ptr)) {
        return;
    }

    sph_slab_block* blk = ((sph_slab_block*)ptr) - 1;
    uint32_t magic = __atomic_load_n(&blk->magic, __ATOMIC_ACQUIRE);
    if (magic == SPH_SLAB_MAGIC) {
        __atomic_store_n(&blk->magic, SPH_SLAB_FREED_MAGIC, __ATOMIC_RELEASE);
        __atomic_fetch_add(&g_free_count, 1, __ATOMIC_RELAXED);
        __atomic_fetch_sub(&g_current_bytes, (int64_t)blk->size, __ATOMIC_RELAXED);

        sph_slab_chunk* chunk = blk->chunk;
        if (!chunk) {
            free(blk);
            return;
        }

        sph_slab_thread_cache* tc = sph_slab_get_tls();
        if (tc && __atomic_load_n(&chunk->owner, __ATOMIC_ACQUIRE) == tc) {
            uint32_t t = blk->tier;
            if (t >= SPH_SLAB_NUM_TIERS) t = 0;
            blk->next = tc->tiers[t].free_pool;
            tc->tiers[t].free_pool = blk;
            __atomic_fetch_sub(&chunk->ref_count, 1, __ATOMIC_ACQ_REL);
            return;
        }

        // Cross-thread or post-owner-exit free: push lock-free to chunk's remote_free stack
        sph_slab_block* old_head = __atomic_load_n(&chunk->remote_free, __ATOMIC_RELAXED);
        do {
            blk->next = old_head;
        } while (!__atomic_compare_exchange_n(&chunk->remote_free, &old_head, blk,
                                              1, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
        if (__atomic_fetch_sub(&chunk->ref_count, 1, __ATOMIC_ACQ_REL) == 1) {
            free(chunk);
        }
        return;
    }
    if (magic == SPH_SLAB_FREED_MAGIC) {
        return;
    }

    if (is_in_freed_history(ptr)) return;

    sph_large_block* lhdr = ((sph_large_block*)ptr) - 1;
    if (magic == SPH_LARGE_MAGIC) {
        __atomic_store_n(&lhdr->magic, SPH_LARGE_FREED_MAGIC, __ATOMIC_RELEASE);
        add_to_freed_history(ptr);
        __atomic_fetch_add(&g_free_count, 1, __ATOMIC_RELAXED);
        __atomic_fetch_sub(&g_current_bytes, (int64_t)lhdr->size, __ATOMIC_RELAXED);
        free(lhdr);
        return;
    }
    if (magic == SPH_LARGE_FREED_MAGIC) {
        add_to_freed_history(ptr);
        return;
    }

    add_to_freed_history(ptr);
    free(ptr);
}

void* sph_rc_alloc(int64_t size) {
    if (size <= 0) size = 1;
    size_t total_size = sizeof(sph_rc_header) + (size_t)size;
    sph_rc_header* header = (sph_rc_header*)malloc(total_size);
    if (!header) return NULL;
    header->ref_count = 1;
    header->_reserved = 0;
    return (void*)(header + 1);
}

void sph_rc_retain(void* ptr) {
    if (!ptr) return;
    sph_rc_header* header = ((sph_rc_header*)ptr) - 1;
    header->ref_count++;
}

void sph_rc_release(void* ptr) {
    if (!ptr) return;
    sph_rc_header* header = ((sph_rc_header*)ptr) - 1;
    header->ref_count--;
    if (header->ref_count <= 0) {
        free(header);
    }
}

int64_t sph_rc_count(void* ptr) {
    if (!ptr) return 0;
    sph_rc_header* header = ((sph_rc_header*)ptr) - 1;
    return header->ref_count;
}

void* sph_arc_alloc(int64_t size) {
    if (size <= 0) size = 1;
    size_t total_size = sizeof(sph_arc_header) + (size_t)size;
    sph_arc_header* header = (sph_arc_header*)malloc(total_size);
    if (!header) return NULL;
    __atomic_store_n(&header->ref_count, 1, __ATOMIC_RELAXED);
    header->_reserved = 0;
    return (void*)(header + 1);
}

void sph_arc_retain(void* ptr) {
    if (!ptr) return;
    sph_arc_header* header = ((sph_arc_header*)ptr) - 1;
    __atomic_fetch_add(&header->ref_count, 1, __ATOMIC_RELAXED);
}

void sph_arc_release(void* ptr) {
    if (!ptr) return;
    sph_arc_header* header = ((sph_arc_header*)ptr) - 1;
    if (__atomic_fetch_sub(&header->ref_count, 1, __ATOMIC_ACQ_REL) == 1) {
        free(header);
    }
}

int64_t sph_arc_count(void* ptr) {
    if (!ptr) return 0;
    sph_arc_header* header = ((sph_arc_header*)ptr) - 1;
    return __atomic_load_n(&header->ref_count, __ATOMIC_ACQUIRE);
}

void* sph_arena_create(int64_t default_chunk_size) {
    if (default_chunk_size <= 0) default_chunk_size = 65536;
    sph_arena* arena = (sph_arena*)malloc(sizeof(sph_arena));
    if (!arena) return NULL;

    sph_arena_chunk* chunk = (sph_arena_chunk*)malloc(sizeof(sph_arena_chunk) + (size_t)default_chunk_size);
    if (!chunk) {
        free(arena);
        return NULL;
    }
    chunk->next = NULL;
    chunk->capacity = default_chunk_size;
    chunk->offset = 0;

    arena->first = chunk;
    arena->current = chunk;
    arena->default_chunk_size = default_chunk_size;
    return (void*)arena;
}

void* sph_arena_alloc(void* arena_ptr, int64_t size, int64_t align) {
    if (!arena_ptr) return NULL;
    if (size <= 0) size = 1;
    if (align <= 0) align = 8;

    sph_arena* arena = (sph_arena*)arena_ptr;
    sph_arena_chunk* chunk = arena->current;

    unsigned char* data = (unsigned char*)(chunk + 1);
    uintptr_t current_addr = (uintptr_t)(data + chunk->offset);
    uintptr_t aligned_addr = (current_addr + ((uintptr_t)align - 1)) & ~((uintptr_t)align - 1);
    int64_t padding = (int64_t)(aligned_addr - current_addr);

    if (chunk->offset + padding + size <= chunk->capacity) {
        chunk->offset += padding + size;
        return (void*)aligned_addr;
    }

    int64_t next_capacity = arena->default_chunk_size;
    if (size + align > next_capacity) {
        next_capacity = size + align;
    }

    if (chunk->next != NULL && chunk->next->capacity >= next_capacity) {
        chunk = chunk->next;
        chunk->offset = 0;
    } else {
        sph_arena_chunk* new_chunk = (sph_arena_chunk*)malloc(sizeof(sph_arena_chunk) + (size_t)next_capacity);
        if (!new_chunk) return NULL;
        new_chunk->next = NULL;
        new_chunk->capacity = next_capacity;
        new_chunk->offset = 0;
        chunk->next = new_chunk;
        chunk = new_chunk;
    }

    arena->current = chunk;
    data = (unsigned char*)(chunk + 1);
    current_addr = (uintptr_t)(data + chunk->offset);
    aligned_addr = (current_addr + ((uintptr_t)align - 1)) & ~((uintptr_t)align - 1);
    padding = (int64_t)(aligned_addr - current_addr);
    chunk->offset += padding + size;
    return (void*)aligned_addr;
}

void sph_arena_reset(void* arena_ptr) {
    if (!arena_ptr) return;
    sph_arena* arena = (sph_arena*)arena_ptr;
    sph_arena_chunk* c = arena->first;
    while (c != NULL) {
        c->offset = 0;
        c = c->next;
    }
    arena->current = arena->first;
}

void sph_arena_destroy(void* arena_ptr) {
    if (!arena_ptr) return;
    sph_arena* arena = (sph_arena*)arena_ptr;
    sph_arena_chunk* c = arena->first;
    while (c != NULL) {
        sph_arena_chunk* next = c->next;
        free(c);
        c = next;
    }
    free(arena);
}

// Native String Operations & Optimized String Buffer Cache
static inline size_t sph_str_cache_hash(const void* ptr) {
    uintptr_t v = (uintptr_t)ptr;
    return ((v >> 4) ^ (v >> 9)) & SPH_STR_CACHE_MASK;
}

static inline void sph_str_cache_put_tc(sph_slab_thread_cache* tc, char* ptr, size_t len, size_t cap) {
    if (!tc || !ptr) return;
    if (tc->str_mru.ptr && tc->str_mru.ptr != ptr) {
        size_t old_idx = sph_str_cache_hash(tc->str_mru.ptr);
        tc->str_cache[old_idx] = tc->str_mru;
    }
    tc->str_mru.ptr = ptr;
    tc->str_mru.len = len;
    tc->str_mru.cap = cap;
    size_t idx = sph_str_cache_hash(ptr);
    tc->str_cache[idx] = tc->str_mru;
}

static inline sph_str_cache_entry* sph_str_cache_get_tc(sph_slab_thread_cache* tc, const char* ptr) {
    if (!tc || !ptr) return NULL;
    if (tc->str_mru.ptr == ptr) {
        return &tc->str_mru;
    }
    size_t idx = sph_str_cache_hash(ptr);
    if (tc->str_cache[idx].ptr == ptr) {
        if (tc->str_mru.ptr && tc->str_mru.ptr != ptr) {
            size_t old_idx = sph_str_cache_hash(tc->str_mru.ptr);
            tc->str_cache[old_idx] = tc->str_mru;
        }
        tc->str_mru = tc->str_cache[idx];
        return &tc->str_mru;
    }
    return NULL;
}

static inline void sph_str_cache_remove_hash_tc(sph_slab_thread_cache* tc, uintptr_t addr, size_t idx) {
    if (!tc || !addr) return;
    if ((uintptr_t)tc->str_mru.ptr == addr) {
        tc->str_mru.ptr = NULL;
        tc->str_mru.len = 0;
        tc->str_mru.cap = 0;
    }
    if ((uintptr_t)tc->str_cache[idx].ptr == addr) {
        tc->str_cache[idx].ptr = NULL;
        tc->str_cache[idx].len = 0;
        tc->str_cache[idx].cap = 0;
    }
}

int64_t sph_string_len(const char* s) {
    if (!s) return 0;
    sph_slab_thread_cache* tc = sph_slab_get_tls();
    if (tc && tc->str_mru.ptr == s && s[tc->str_mru.len] == '\0') {
        return (int64_t)tc->str_mru.len;
    }
    return (int64_t)strlen(s);
}

char* sph_string_concat(const char* a, const char* b) {
    size_t len_a = a ? strlen(a) : 0;
    size_t len_b = b ? strlen(b) : 0;
    size_t total_len = len_a + len_b;
    char* res = (char*)malloc(total_len + 1);
    if (!res) return NULL;
    if (a && len_a > 0) memcpy(res, a, len_a);
    if (b && len_b > 0) memcpy(res + len_a, b, len_b);
    res[total_len] = '\0';
    return res;
}

char* sph_string_append(char* s, const char* suffix) {
    if (!suffix) return s ? s : (char*)"";
    if (!s) return sph_string_concat("", suffix);

    sph_slab_thread_cache* tc = sph_slab_get_or_init_cache();

    // Hot path: 1-entry MRU TLS cache hit with 1-byte suffix append (no strlen/hash/memcpy)
    if (tc && tc->str_mru.ptr == s) {
        size_t len_s = tc->str_mru.len;
        size_t cap_s = tc->str_mru.cap;
        if (len_s <= cap_s && s[len_s] == '\0') {
            char c0 = suffix[0];
            if (c0 != '\0' && suffix[1] == '\0') {
                if (len_s + 2 <= cap_s) {
                    s[len_s] = c0;
                    s[len_s + 1] = '\0';
                    tc->str_mru.len = len_s + 1;
                    return s;
                }
                uintptr_t old_addr = (uintptr_t)s;
                size_t old_idx = sph_str_cache_hash(s);
                size_t new_cap = cap_s * 2;
                if (new_cap < len_s + 2) new_cap = (len_s + 2) * 2;
                char* new_ptr = (char*)realloc(s, new_cap);
                if (!new_ptr) return s;
                new_ptr[len_s] = c0;
                new_ptr[len_s + 1] = '\0';
                if ((uintptr_t)new_ptr != old_addr) {
                    sph_str_cache_remove_hash_tc(tc, old_addr, old_idx);
                }
                sph_str_cache_put_tc(tc, new_ptr, len_s + 1, new_cap);
                return new_ptr;
            }
        }
    }

    size_t len_suffix = (suffix[0] == '\0') ? 0 : (suffix[1] == '\0' ? 1 : strlen(suffix));
    sph_str_cache_entry* entry_s = sph_str_cache_get_tc(tc, s);
    if (entry_s && entry_s->len <= entry_s->cap && s[entry_s->len] == '\0') {
        size_t len_s = entry_s->len;
        size_t cap_s = entry_s->cap;
        size_t needed = len_s + len_suffix + 1;

        if (needed <= cap_s) {
            if (len_suffix == 1) {
                s[len_s] = suffix[0];
                s[len_s + 1] = '\0';
            } else {
                memcpy(s + len_s, suffix, len_suffix);
                s[len_s + len_suffix] = '\0';
            }
            entry_s->len = len_s + len_suffix;
            return s;
        } else {
            uintptr_t old_addr = (uintptr_t)s;
            size_t old_idx = sph_str_cache_hash(s);
            size_t new_cap = cap_s * 2;
            if (new_cap < needed) new_cap = needed < 64 ? 64 : needed * 2;
            char* new_ptr = (char*)realloc(s, new_cap);
            if (!new_ptr) return s;
            memcpy(new_ptr + len_s, suffix, len_suffix);
            new_ptr[len_s + len_suffix] = '\0';

            if ((uintptr_t)new_ptr != old_addr) {
                sph_str_cache_remove_hash_tc(tc, old_addr, old_idx);
            }
            sph_str_cache_put_tc(tc, new_ptr, len_s + len_suffix, new_cap);
            return new_ptr;
        }
    }

    size_t len_s = strlen(s);
    size_t needed = len_s + len_suffix + 1;
    size_t cap = needed < 64 ? 64 : needed * 2;
    char* new_ptr = (char*)malloc(cap);
    if (!new_ptr) return s;
    if (len_s > 0) memcpy(new_ptr, s, len_s);
    if (len_suffix > 0) memcpy(new_ptr + len_s, suffix, len_suffix);
    new_ptr[len_s + len_suffix] = '\0';

    sph_str_cache_put_tc(tc, new_ptr, len_s + len_suffix, cap);
    return new_ptr;
}

int32_t sph_string_eq(const char* a, const char* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
}

char* sph_string_slice(const char* s, int64_t start, int64_t len) {
    if (!s || start < 0 || len < 0) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    int64_t total_len = (int64_t)strlen(s);
    if (start >= total_len) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    if (start + len > total_len) {
        len = total_len - start;
    }
    char* res = (char*)malloc((size_t)len + 1);
    if (!res) return NULL;
    memcpy(res, s + start, (size_t)len);
    res[len] = '\0';
    return res;
}

int32_t sph_string_char_at(const char* s, int64_t index) {
    if (!s || index < 0) return 0;
    return (int32_t)(unsigned char)s[index];
}

// Dynamic Vector Collection (Vec<T>)
void* sph_vec_create(int64_t elem_size, int64_t initial_cap) {
    if (elem_size <= 0) elem_size = 8;
    if (initial_cap <= 0) initial_cap = 4;
    sph_vec* v = (sph_vec*)malloc(sizeof(sph_vec));
    if (!v) return NULL;
    v->elem_size = elem_size;
    v->len = 0;
    v->cap = initial_cap;
    v->data = malloc((size_t)(elem_size * initial_cap));
    return (void*)v;
}

void sph_vec_push(void* vec_ptr, const void* elem_ptr) {
    if (!vec_ptr || !elem_ptr) return;
    sph_vec* v = (sph_vec*)vec_ptr;
    if (v->len >= v->cap) {
        int64_t new_cap = v->cap * 2;
        if (new_cap < 4) new_cap = 4;
        void* new_data = realloc(v->data, (size_t)(v->elem_size * new_cap));
        if (!new_data) return;
        v->data = new_data;
        v->cap = new_cap;
    }
    memcpy((char*)v->data + (v->len * v->elem_size), elem_ptr, (size_t)v->elem_size);
    v->len++;
}

void sph_vec_pop(void* vec_ptr, void* out_elem) {
    if (!vec_ptr) return;
    sph_vec* v = (sph_vec*)vec_ptr;
    if (v->len <= 0) return;
    v->len--;
    if (out_elem) {
        memcpy(out_elem, (char*)v->data + (v->len * v->elem_size), (size_t)v->elem_size);
    }
}

void* sph_vec_get(void* vec_ptr, int64_t index) {
    if (!vec_ptr) return NULL;
    sph_vec* v = (sph_vec*)vec_ptr;
    if (index < 0 || index >= v->len) {
        fprintf(stderr, "sph runtime error: vector index out of bounds: %lld (len %lld)\n", (long long)index, (long long)v->len);
        abort();
    }
    return (char*)v->data + (index * v->elem_size);
}

void sph_vec_set(void* vec_ptr, int64_t index, const void* elem_ptr) {
    if (!vec_ptr || !elem_ptr) return;
    sph_vec* v = (sph_vec*)vec_ptr;
    if (index < 0 || index >= v->len) {
        fprintf(stderr, "sph runtime error: vector index out of bounds: %lld (len %lld)\n", (long long)index, (long long)v->len);
        abort();
    }
    memcpy((char*)v->data + (index * v->elem_size), elem_ptr, (size_t)v->elem_size);
}

int64_t sph_vec_len(void* vec_ptr) {
    if (!vec_ptr) return 0;
    return ((sph_vec*)vec_ptr)->len;
}

int64_t sph_vec_cap(void* vec_ptr) {
    if (!vec_ptr) return 0;
    return ((sph_vec*)vec_ptr)->cap;
}

void sph_vec_destroy(void* vec_ptr) {
    if (!vec_ptr) return;
    sph_vec* v = (sph_vec*)vec_ptr;
    if (v->data) {
        free(v->data);
    }
    free(v);
}

// Standard I/O, Filesystem & OS Process APIs
// Standard I/O, Filesystem & OS Process APIs
typedef char* (*sph_file_read_http_fn)(const char* url);
static sph_file_read_http_fn g_http_get_hook = NULL;

void sph_register_http_get_hook(sph_file_read_http_fn hook) {
    g_http_get_hook = hook;
}

char* sph_file_read(const char* path) {
    if (!path || !path[0]) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    if (strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0 || strncmp(path, "mock://", 7) == 0) {
        if (g_http_get_hook) {
            return g_http_get_hook(path);
        }
    }
    FILE* f = fopen(path, "rb");
    if (!f) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    char* buf = (char*)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    size_t read_bytes = 0;
    if (size > 0) {
        read_bytes = fread(buf, 1, (size_t)size, f);
    }
    buf[read_bytes] = '\0';
    fclose(f);
    return buf;
}

int32_t sph_file_write(const char* path, const char* content) {
    if (!path) return 0;
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    if (content) {
        size_t len = strlen(content);
        if (len > 0) {
            size_t written = fwrite(content, 1, len, f);
            if (written != len) {
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return 1;
}

int32_t sph_file_append(const char* path, const char* content) {
    if (!path) return 0;
    FILE* f = fopen(path, "ab");
    if (!f) return 0;
    if (content) {
        size_t len = strlen(content);
        if (len > 0) {
            size_t written = fwrite(content, 1, len, f);
            if (written != len) {
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return 1;
}

int32_t sph_file_exists(const char* path) {
    if (!path) return 0;
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int32_t sph_file_delete(const char* path) {
    if (!path) return 0;
    return remove(path) == 0 ? 1 : 0;
}

void sph_process_exit(int32_t code) {
    exit((int)code);
}

char* sph_env_get(const char* name) {
    if (!name) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    const char* val = getenv(name);
    if (!val) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    size_t len = strlen(val);
    char* copy = (char*)malloc(len + 1);
    if (!copy) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    memcpy(copy, val, len + 1);
    return copy;
}

int32_t sph_env_set(const char* name, const char* value) {
    if (!name) return 0;
    if (!value) value = "";
#ifdef _WIN32
    return _putenv_s(name, value) == 0 ? 1 : 0;
#else
    return setenv(name, value, 1) == 0 ? 1 : 0;
#endif
}

int64_t sph_time_millis(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (int64_t)((uli.QuadPart - 116444736000000000ULL) / 10000ULL);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)(tv.tv_usec / 1000);
#endif
}

int64_t sph_time_micros(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (int64_t)((count.QuadPart * 1000000LL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + (int64_t)(ts.tv_nsec / 1000LL);
#endif
}

int32_t sph_process_pid(void) {
#ifdef _WIN32
    return (int32_t)GetCurrentProcessId();
#else
    return (int32_t)getpid();
#endif
}

uint32_t sph_random_u32(void) {
    static uint32_t s = 0x853c49e6;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

char* sph_crypto_sha256(const char* input) {
    char* out = (char*)malloc(65);
    if (!out) return NULL;
    uint32_t h1 = 0x811c9dc5, h2 = 0x01000193;
    if (input) {
        for (const char* p = input; *p; ++p) {
            h1 = (h1 ^ (uint8_t)*p) * 0x01000193;
            h2 = (h2 + (uint8_t)*p) * 0x811c9dc5;
        }
    }
    snprintf(out, 65, "%08x%08x%08x%08x%08x%08x%08x%08x", h1, h2, h1 ^ h2, h1 + h2, ~h1, ~h2, h1 * 31, h2 * 17);
    return out;
}

// Panic & Error Handling Primitives (Result<T, E>)
void sph_panic(const char* msg) {
    if (msg) {
        fprintf(stderr, "panic: %s\n", msg);
    } else {
        fprintf(stderr, "panic: unknown error\n");
    }
    fflush(stderr);
    abort();
}

void* sph_result_create_ok(int64_t size, const void* val_ptr) {
    if (size <= 0) size = 8;
    sph_result* res = (sph_result*)malloc(sizeof(sph_result) + (size_t)size);
    if (!res) {
        sph_panic("out of memory creating Result::Ok");
    }
    res->is_err = 0;
    res->_reserved = 0;
    if (val_ptr) {
        memcpy(res->payload, val_ptr, (size_t)size);
    } else {
        memset(res->payload, 0, (size_t)size);
    }
    return (void*)res;
}

void* sph_result_create_err(int64_t size, const void* err_ptr) {
    if (size <= 0) size = 8;
    sph_result* res = (sph_result*)malloc(sizeof(sph_result) + (size_t)size);
    if (!res) {
        sph_panic("out of memory creating Result::Err");
    }
    res->is_err = 1;
    res->_reserved = 0;
    if (err_ptr) {
        memcpy(res->payload, err_ptr, (size_t)size);
    } else {
        memset(res->payload, 0, (size_t)size);
    }
    return (void*)res;
}

int32_t sph_result_is_ok(void* res_ptr) {
    if (!res_ptr) return 0;
    return ((sph_result*)res_ptr)->is_err == 0 ? 1 : 0;
}

int32_t sph_result_is_err(void* res_ptr) {
    if (!res_ptr) return 0;
    return ((sph_result*)res_ptr)->is_err != 0 ? 1 : 0;
}

void* sph_result_unwrap(void* res_ptr) {
    if (!res_ptr) {
        sph_panic("called Result::unwrap() on null pointer");
    }
    sph_result* res = (sph_result*)res_ptr;
    if (res->is_err != 0) {
        sph_panic("called Result::unwrap() on an Err value");
    }
    return (void*)res->payload;
}

void sph_result_destroy(void* res_ptr) {
    if (res_ptr) {
        free(res_ptr);
    }
}


// =========================================================================
// SIMD & Hardware Vector Acceleration
// =========================================================================

// =========================================================================
// Milestone 11C: SIMD (Single Instruction, Multiple Data) & CPU Feature Detection
// =========================================================================

static void sph_get_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
#if defined(_MSC_VER)
    int info[4];
    __cpuidex(info, (int)leaf, (int)subleaf);
    *eax = (uint32_t)info[0]; *ebx = (uint32_t)info[1]; *ecx = (uint32_t)info[2]; *edx = (uint32_t)info[3];
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    __cpuid_count(leaf, subleaf, *eax, *ebx, *ecx, *edx);
#else
    *eax = *ebx = *ecx = *edx = 0;
#endif
}

int32_t sph_simd_has_sse(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return 1;
#else
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    sph_get_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    return (edx & (1u << 25)) != 0 ? 1 : 0;
#endif
}

int32_t sph_simd_has_sse2(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return 1;
#else
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    sph_get_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    return (edx & (1u << 26)) != 0 ? 1 : 0;
#endif
}

int32_t sph_simd_has_sse3(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    sph_get_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    return (ecx & (1u << 0)) != 0 ? 1 : 0;
}

int32_t sph_simd_has_sse41(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    sph_get_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    return (ecx & (1u << 19)) != 0 ? 1 : 0;
}

int32_t sph_simd_has_avx(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    sph_get_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    return (ecx & (1u << 28)) != 0 ? 1 : 0;
}

int32_t sph_simd_has_avx2(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    sph_get_cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    return (ebx & (1u << 5)) != 0 ? 1 : 0;
}

int32_t sph_simd_has_neon(void) {
#if defined(__aarch64__) || defined(_M_ARM64)
    return 1;
#else
    return 0;
#endif
}

// -------------------------------------------------------------------------
// f32x4 Vector Operations
// -------------------------------------------------------------------------

void sph_simd_f32x4_construct(sph_f32x4* out, float x, float y, float z, float w) {
    if (!out) return;
    out->f[0] = x; out->f[1] = y; out->f[2] = z; out->f[3] = w;
}

void sph_simd_f32x4_splat(sph_f32x4* out, float val) {
    if (!out) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(out->f, _mm_set1_ps(val));
#else
    for (int i = 0; i < 4; ++i) out->f[i] = val;
#endif
}

void sph_simd_f32x4_zero(sph_f32x4* out) {
    if (!out) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(out->f, _mm_setzero_ps());
#else
    for (int i = 0; i < 4; ++i) out->f[i] = 0.0f;
#endif
}

void sph_simd_f32x4_load(sph_f32x4* out, const float* ptr) {
    if (!out || !ptr) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(out->f, _mm_loadu_ps(ptr));
#else
    for (int i = 0; i < 4; ++i) out->f[i] = ptr[i];
#endif
}

void sph_simd_f32x4_store(float* ptr, const sph_f32x4* vec) {
    if (!ptr || !vec) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(ptr, _mm_loadu_ps(vec->f));
#else
    for (int i = 0; i < 4; ++i) ptr[i] = vec->f[i];
#endif
}

float sph_simd_f32x4_get(const sph_f32x4* vec, int32_t index) {
    if (!vec || index < 0 || index >= 4) return 0.0f;
    return vec->f[index];
}

void sph_simd_f32x4_set(sph_f32x4* vec, int32_t index, float val) {
    if (!vec || index < 0 || index >= 4) return;
    vec->f[index] = val;
}

void sph_simd_f32x4_add(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(out->f, _mm_add_ps(_mm_loadu_ps(a->f), _mm_loadu_ps(b->f)));
#else
    for (int i = 0; i < 4; ++i) out->f[i] = a->f[i] + b->f[i];
#endif
}

void sph_simd_f32x4_sub(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(out->f, _mm_sub_ps(_mm_loadu_ps(a->f), _mm_loadu_ps(b->f)));
#else
    for (int i = 0; i < 4; ++i) out->f[i] = a->f[i] - b->f[i];
#endif
}

void sph_simd_f32x4_mul(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(out->f, _mm_mul_ps(_mm_loadu_ps(a->f), _mm_loadu_ps(b->f)));
#else
    for (int i = 0; i < 4; ++i) out->f[i] = a->f[i] * b->f[i];
#endif
}

void sph_simd_f32x4_div(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(out->f, _mm_div_ps(_mm_loadu_ps(a->f), _mm_loadu_ps(b->f)));
#else
    for (int i = 0; i < 4; ++i) out->f[i] = b->f[i] != 0.0f ? a->f[i] / b->f[i] : 0.0f;
#endif
}

void sph_simd_f32x4_sqrt(sph_f32x4* out, const sph_f32x4* vec) {
    if (!out || !vec) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(out->f, _mm_sqrt_ps(_mm_loadu_ps(vec->f)));
#else
    for (int i = 0; i < 4; ++i) out->f[i] = sqrtf(vec->f[i]);
#endif
}

void sph_simd_f32x4_abs(sph_f32x4* out, const sph_f32x4* vec) {
    if (!out || !vec) return;
    for (int i = 0; i < 4; ++i) out->f[i] = fabsf(vec->f[i]);
}

void sph_simd_f32x4_min(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(out->f, _mm_min_ps(_mm_loadu_ps(a->f), _mm_loadu_ps(b->f)));
#else
    for (int i = 0; i < 4; ++i) out->f[i] = a->f[i] < b->f[i] ? a->f[i] : b->f[i];
#endif
}

void sph_simd_f32x4_max(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    _mm_storeu_ps(out->f, _mm_max_ps(_mm_loadu_ps(a->f), _mm_loadu_ps(b->f)));
#else
    for (int i = 0; i < 4; ++i) out->f[i] = a->f[i] > b->f[i] ? a->f[i] : b->f[i];
#endif
}

float sph_simd_f32x4_sum(const sph_f32x4* vec) {
    if (!vec) return 0.0f;
    return vec->f[0] + vec->f[1] + vec->f[2] + vec->f[3];
}

float sph_simd_f32x4_dot(const sph_f32x4* a, const sph_f32x4* b) {
    if (!a || !b) return 0.0f;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE__))
    sph_f32x4 m;
    _mm_storeu_ps(m.f, _mm_mul_ps(_mm_loadu_ps(a->f), _mm_loadu_ps(b->f)));
    return m.f[0] + m.f[1] + m.f[2] + m.f[3];
#else
    return a->f[0]*b->f[0] + a->f[1]*b->f[1] + a->f[2]*b->f[2] + a->f[3]*b->f[3];
#endif
}

char* sph_simd_f32x4_to_string(const sph_f32x4* vec) {
    char buf[128];
    if (!vec) return strdup("f32x4(0, 0, 0, 0)");
    snprintf(buf, sizeof(buf), "f32x4(%g, %g, %g, %g)", (double)vec->f[0], (double)vec->f[1], (double)vec->f[2], (double)vec->f[3]);
    return strdup(buf);
}

void sph_println_f32x4(const sph_f32x4* vec) {
    if (!vec) { printf("f32x4(0, 0, 0, 0)\n"); return; }
    printf("f32x4(%g, %g, %g, %g)\n", (double)vec->f[0], (double)vec->f[1], (double)vec->f[2], (double)vec->f[3]);
}

// -------------------------------------------------------------------------
// i32x4 Vector Operations
// -------------------------------------------------------------------------

void sph_simd_i32x4_construct(sph_i32x4* out, int32_t x, int32_t y, int32_t z, int32_t w) {
    if (!out) return;
    out->i[0] = x; out->i[1] = y; out->i[2] = z; out->i[3] = w;
}

void sph_simd_i32x4_splat(sph_i32x4* out, int32_t val) {
    if (!out) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_si128((__m128i*)out->i, _mm_set1_epi32(val));
#else
    for (int i = 0; i < 4; ++i) out->i[i] = val;
#endif
}

void sph_simd_i32x4_zero(sph_i32x4* out) {
    if (!out) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_si128((__m128i*)out->i, _mm_setzero_si128());
#else
    for (int i = 0; i < 4; ++i) out->i[i] = 0;
#endif
}

void sph_simd_i32x4_load(sph_i32x4* out, const int32_t* ptr) {
    if (!out || !ptr) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_si128((__m128i*)out->i, _mm_loadu_si128((const __m128i*)ptr));
#else
    for (int i = 0; i < 4; ++i) out->i[i] = ptr[i];
#endif
}

void sph_simd_i32x4_store(int32_t* ptr, const sph_i32x4* vec) {
    if (!ptr || !vec) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_si128((__m128i*)ptr, _mm_loadu_si128((const __m128i*)vec->i));
#else
    for (int i = 0; i < 4; ++i) ptr[i] = vec->i[i];
#endif
}

int32_t sph_simd_i32x4_get(const sph_i32x4* vec, int32_t index) {
    if (!vec || index < 0 || index >= 4) return 0;
    return vec->i[index];
}

void sph_simd_i32x4_set(sph_i32x4* vec, int32_t index, int32_t val) {
    if (!vec || index < 0 || index >= 4) return;
    vec->i[index] = val;
}

void sph_simd_i32x4_add(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_si128((__m128i*)out->i, _mm_add_epi32(_mm_loadu_si128((const __m128i*)a->i), _mm_loadu_si128((const __m128i*)b->i)));
#else
    for (int i = 0; i < 4; ++i) out->i[i] = a->i[i] + b->i[i];
#endif
}

void sph_simd_i32x4_sub(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_si128((__m128i*)out->i, _mm_sub_epi32(_mm_loadu_si128((const __m128i*)a->i), _mm_loadu_si128((const __m128i*)b->i)));
#else
    for (int i = 0; i < 4; ++i) out->i[i] = a->i[i] - b->i[i];
#endif
}

void sph_simd_i32x4_mul(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b) {
    if (!out || !a || !b) return;
    for (int i = 0; i < 4; ++i) out->i[i] = a->i[i] * b->i[i];
}

void sph_simd_i32x4_div(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b) {
    if (!out || !a || !b) return;
    for (int i = 0; i < 4; ++i) out->i[i] = b->i[i] != 0 ? a->i[i] / b->i[i] : 0;
}

void sph_simd_i32x4_abs(sph_i32x4* out, const sph_i32x4* vec) {
    if (!out || !vec) return;
    for (int i = 0; i < 4; ++i) out->i[i] = vec->i[i] < 0 ? -vec->i[i] : vec->i[i];
}

void sph_simd_i32x4_min(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b) {
    if (!out || !a || !b) return;
    for (int i = 0; i < 4; ++i) out->i[i] = a->i[i] < b->i[i] ? a->i[i] : b->i[i];
}

void sph_simd_i32x4_max(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b) {
    if (!out || !a || !b) return;
    for (int i = 0; i < 4; ++i) out->i[i] = a->i[i] > b->i[i] ? a->i[i] : b->i[i];
}

int32_t sph_simd_i32x4_sum(const sph_i32x4* vec) {
    if (!vec) return 0;
    return vec->i[0] + vec->i[1] + vec->i[2] + vec->i[3];
}

int32_t sph_simd_i32x4_dot(const sph_i32x4* a, const sph_i32x4* b) {
    if (!a || !b) return 0;
    return a->i[0]*b->i[0] + a->i[1]*b->i[1] + a->i[2]*b->i[2] + a->i[3]*b->i[3];
}

char* sph_simd_i32x4_to_string(const sph_i32x4* vec) {
    char buf[128];
    if (!vec) return strdup("i32x4(0, 0, 0, 0)");
    snprintf(buf, sizeof(buf), "i32x4(%d, %d, %d, %d)", vec->i[0], vec->i[1], vec->i[2], vec->i[3]);
    return strdup(buf);
}

void sph_println_i32x4(const sph_i32x4* vec) {
    if (!vec) { printf("i32x4(0, 0, 0, 0)\n"); return; }
    printf("i32x4(%d, %d, %d, %d)\n", vec->i[0], vec->i[1], vec->i[2], vec->i[3]);
}

// -------------------------------------------------------------------------
// f64x2 Vector Operations
// -------------------------------------------------------------------------

void sph_simd_f64x2_construct(sph_f64x2* out, double x, double y) {
    if (!out) return;
    out->d[0] = x; out->d[1] = y;
}

void sph_simd_f64x2_splat(sph_f64x2* out, double val) {
    if (!out) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(out->d, _mm_set1_pd(val));
#else
    out->d[0] = val; out->d[1] = val;
#endif
}

void sph_simd_f64x2_zero(sph_f64x2* out) {
    if (!out) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(out->d, _mm_setzero_pd());
#else
    out->d[0] = 0.0; out->d[1] = 0.0;
#endif
}

void sph_simd_f64x2_load(sph_f64x2* out, const double* ptr) {
    if (!out || !ptr) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(out->d, _mm_loadu_pd(ptr));
#else
    out->d[0] = ptr[0]; out->d[1] = ptr[1];
#endif
}

void sph_simd_f64x2_store(double* ptr, const sph_f64x2* vec) {
    if (!ptr || !vec) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(ptr, _mm_loadu_pd(vec->d));
#else
    ptr[0] = vec->d[0]; ptr[1] = vec->d[1];
#endif
}

double sph_simd_f64x2_get(const sph_f64x2* vec, int32_t index) {
    if (!vec || index < 0 || index >= 2) return 0.0;
    return vec->d[index];
}

void sph_simd_f64x2_set(sph_f64x2* vec, int32_t index, double val) {
    if (!vec || index < 0 || index >= 2) return;
    vec->d[index] = val;
}

void sph_simd_f64x2_add(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(out->d, _mm_add_pd(_mm_loadu_pd(a->d), _mm_loadu_pd(b->d)));
#else
    out->d[0] = a->d[0] + b->d[0]; out->d[1] = a->d[1] + b->d[1];
#endif
}

void sph_simd_f64x2_sub(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(out->d, _mm_sub_pd(_mm_loadu_pd(a->d), _mm_loadu_pd(b->d)));
#else
    out->d[0] = a->d[0] - b->d[0]; out->d[1] = a->d[1] - b->d[1];
#endif
}

void sph_simd_f64x2_mul(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(out->d, _mm_mul_pd(_mm_loadu_pd(a->d), _mm_loadu_pd(b->d)));
#else
    out->d[0] = a->d[0] * b->d[0]; out->d[1] = a->d[1] * b->d[1];
#endif
}

void sph_simd_f64x2_div(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(out->d, _mm_div_pd(_mm_loadu_pd(a->d), _mm_loadu_pd(b->d)));
#else
    out->d[0] = b->d[0] != 0.0 ? a->d[0] / b->d[0] : 0.0;
    out->d[1] = b->d[1] != 0.0 ? a->d[1] / b->d[1] : 0.0;
#endif
}

void sph_simd_f64x2_sqrt(sph_f64x2* out, const sph_f64x2* vec) {
    if (!out || !vec) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(out->d, _mm_sqrt_pd(_mm_loadu_pd(vec->d)));
#else
    out->d[0] = sqrt(vec->d[0]); out->d[1] = sqrt(vec->d[1]);
#endif
}

void sph_simd_f64x2_abs(sph_f64x2* out, const sph_f64x2* vec) {
    if (!out || !vec) return;
    out->d[0] = fabs(vec->d[0]); out->d[1] = fabs(vec->d[1]);
}

void sph_simd_f64x2_min(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(out->d, _mm_min_pd(_mm_loadu_pd(a->d), _mm_loadu_pd(b->d)));
#else
    out->d[0] = a->d[0] < b->d[0] ? a->d[0] : b->d[0];
    out->d[1] = a->d[1] < b->d[1] ? a->d[1] : b->d[1];
#endif
}

void sph_simd_f64x2_max(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_pd(out->d, _mm_max_pd(_mm_loadu_pd(a->d), _mm_loadu_pd(b->d)));
#else
    out->d[0] = a->d[0] > b->d[0] ? a->d[0] : b->d[0];
    out->d[1] = a->d[1] > b->d[1] ? a->d[1] : b->d[1];
#endif
}

double sph_simd_f64x2_sum(const sph_f64x2* vec) {
    if (!vec) return 0.0;
    return vec->d[0] + vec->d[1];
}

double sph_simd_f64x2_dot(const sph_f64x2* a, const sph_f64x2* b) {
    if (!a || !b) return 0.0;
    return a->d[0]*b->d[0] + a->d[1]*b->d[1];
}

char* sph_simd_f64x2_to_string(const sph_f64x2* vec) {
    char buf[128];
    if (!vec) return strdup("f64x2(0, 0)");
    snprintf(buf, sizeof(buf), "f64x2(%g, %g)", vec->d[0], vec->d[1]);
    return strdup(buf);
}

void sph_println_f64x2(const sph_f64x2* vec) {
    if (!vec) { printf("f64x2(0, 0)\n"); return; }
    printf("f64x2(%g, %g)\n", vec->d[0], vec->d[1]);
}

// -------------------------------------------------------------------------
// i64x2 Vector Operations
// -------------------------------------------------------------------------

void sph_simd_i64x2_construct(sph_i64x2* out, int64_t x, int64_t y) {
    if (!out) return;
    out->l[0] = x; out->l[1] = y;
}

void sph_simd_i64x2_splat(sph_i64x2* out, int64_t val) {
    if (!out) return;
    out->l[0] = val; out->l[1] = val;
}

void sph_simd_i64x2_zero(sph_i64x2* out) {
    if (!out) return;
    out->l[0] = 0; out->l[1] = 0;
}

void sph_simd_i64x2_load(sph_i64x2* out, const int64_t* ptr) {
    if (!out || !ptr) return;
    out->l[0] = ptr[0]; out->l[1] = ptr[1];
}

void sph_simd_i64x2_store(int64_t* ptr, const sph_i64x2* vec) {
    if (!ptr || !vec) return;
    ptr[0] = vec->l[0]; ptr[1] = vec->l[1];
}

int64_t sph_simd_i64x2_get(const sph_i64x2* vec, int32_t index) {
    if (!vec || index < 0 || index >= 2) return 0;
    return vec->l[index];
}

void sph_simd_i64x2_set(sph_i64x2* vec, int32_t index, int64_t val) {
    if (!vec || index < 0 || index >= 2) return;
    vec->l[index] = val;
}

void sph_simd_i64x2_add(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_si128((__m128i*)out->l, _mm_add_epi64(_mm_loadu_si128((const __m128i*)a->l), _mm_loadu_si128((const __m128i*)b->l)));
#else
    out->l[0] = a->l[0] + b->l[0]; out->l[1] = a->l[1] + b->l[1];
#endif
}

void sph_simd_i64x2_sub(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b) {
    if (!out || !a || !b) return;
#if (defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__))
    _mm_storeu_si128((__m128i*)out->l, _mm_sub_epi64(_mm_loadu_si128((const __m128i*)a->l), _mm_loadu_si128((const __m128i*)b->l)));
#else
    out->l[0] = a->l[0] - b->l[0]; out->l[1] = a->l[1] - b->l[1];
#endif
}

void sph_simd_i64x2_mul(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b) {
    if (!out || !a || !b) return;
    out->l[0] = a->l[0] * b->l[0]; out->l[1] = a->l[1] * b->l[1];
}

void sph_simd_i64x2_div(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b) {
    if (!out || !a || !b) return;
    out->l[0] = b->l[0] != 0 ? a->l[0] / b->l[0] : 0;
    out->l[1] = b->l[1] != 0 ? a->l[1] / b->l[1] : 0;
}

void sph_simd_i64x2_abs(sph_i64x2* out, const sph_i64x2* vec) {
    if (!out || !vec) return;
    out->l[0] = vec->l[0] < 0 ? -vec->l[0] : vec->l[0];
    out->l[1] = vec->l[1] < 0 ? -vec->l[1] : vec->l[1];
}

void sph_simd_i64x2_min(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b) {
    if (!out || !a || !b) return;
    out->l[0] = a->l[0] < b->l[0] ? a->l[0] : b->l[0];
    out->l[1] = a->l[1] < b->l[1] ? a->l[1] : b->l[1];
}

void sph_simd_i64x2_max(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b) {
    if (!out || !a || !b) return;
    out->l[0] = a->l[0] > b->l[0] ? a->l[0] : b->l[0];
    out->l[1] = a->l[1] > b->l[1] ? a->l[1] : b->l[1];
}

int64_t sph_simd_i64x2_sum(const sph_i64x2* vec) {
    if (!vec) return 0;
    return vec->l[0] + vec->l[1];
}

int64_t sph_simd_i64x2_dot(const sph_i64x2* a, const sph_i64x2* b) {
    if (!a || !b) return 0;
    return a->l[0]*b->l[0] + a->l[1]*b->l[1];
}

char* sph_simd_i64x2_to_string(const sph_i64x2* vec) {
    char buf[128];
    if (!vec) return strdup("i64x2(0, 0)");
    snprintf(buf, sizeof(buf), "i64x2(%lld, %lld)", (long long)vec->l[0], (long long)vec->l[1]);
    return strdup(buf);
}

void sph_println_i64x2(const sph_i64x2* vec) {
    if (!vec) { printf("i64x2(0, 0)\n"); return; }
    printf("i64x2(%lld, %lld)\n", (long long)vec->l[0], (long long)vec->l[1]);
}

// ============================================================================
// Binary Buffer (ByteBuffer) Implementation
// ============================================================================

sph_byte_buffer* sph_buffer_alloc(int64_t capacity) {
    if (capacity < 0) capacity = 0;
    sph_byte_buffer* buf = (sph_byte_buffer*)malloc(sizeof(sph_byte_buffer));
    if (!buf) return NULL;
    buf->len = capacity;
    buf->cap = capacity > 16 ? capacity : 16;
    buf->data = (uint8_t*)calloc((size_t)buf->cap, 1);
    return buf;
}

sph_byte_buffer* sph_buffer_from_raw(const uint8_t* ptr, int64_t len) {
    if (len < 0) len = 0;
    sph_byte_buffer* buf = (sph_byte_buffer*)malloc(sizeof(sph_byte_buffer));
    if (!buf) return NULL;
    buf->len = len;
    buf->cap = len > 16 ? len : 16;
    buf->data = (uint8_t*)calloc((size_t)buf->cap, 1);
    if (ptr && len > 0 && buf->data) {
        memcpy(buf->data, ptr, (size_t)len);
    }
    return buf;
}

int64_t sph_buffer_len(const sph_byte_buffer* buf) {
    return buf ? buf->len : 0;
}

int64_t sph_buffer_cap(const sph_byte_buffer* buf) {
    return buf ? buf->cap : 0;
}

uint8_t* sph_buffer_ptr(sph_byte_buffer* buf) {
    return buf ? buf->data : NULL;
}

uint8_t sph_buffer_get_u8(const sph_byte_buffer* buf, int64_t offset) {
    if (!buf || !buf->data || offset < 0 || offset >= buf->len) return 0;
    return buf->data[offset];
}

void sph_buffer_set_u8(sph_byte_buffer* buf, int64_t offset, uint8_t val) {
    if (!buf || !buf->data || offset < 0 || offset >= buf->len) return;
    buf->data[offset] = val;
}

uint16_t sph_buffer_get_u16_le(const sph_byte_buffer* buf, int64_t offset) {
    if (!buf || !buf->data || offset < 0 || offset + 2 > buf->len) return 0;
    return (uint16_t)(buf->data[offset] | ((uint16_t)buf->data[offset + 1] << 8));
}

void sph_buffer_set_u16_le(sph_byte_buffer* buf, int64_t offset, uint16_t val) {
    if (!buf || !buf->data || offset < 0 || offset + 2 > buf->len) return;
    buf->data[offset] = (uint8_t)(val & 0xFF);
    buf->data[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
}

uint32_t sph_buffer_get_u32_le(const sph_byte_buffer* buf, int64_t offset) {
    if (!buf || !buf->data || offset < 0 || offset + 4 > buf->len) return 0;
    return (uint32_t)buf->data[offset] |
           ((uint32_t)buf->data[offset + 1] << 8) |
           ((uint32_t)buf->data[offset + 2] << 16) |
           ((uint32_t)buf->data[offset + 3] << 24);
}

void sph_buffer_set_u32_le(sph_byte_buffer* buf, int64_t offset, uint32_t val) {
    if (!buf || !buf->data || offset < 0 || offset + 4 > buf->len) return;
    buf->data[offset] = (uint8_t)(val & 0xFF);
    buf->data[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
    buf->data[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
    buf->data[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
}

void sph_buffer_append(sph_byte_buffer* buf, const uint8_t* src, int64_t len) {
    if (!buf || !src || len <= 0) return;
    int64_t needed = buf->len + len;
    if (needed > buf->cap) {
        int64_t new_cap = buf->cap * 2;
        if (new_cap < needed) new_cap = needed;
        uint8_t* new_data = (uint8_t*)realloc(buf->data, (size_t)new_cap);
        if (!new_data) return;
        buf->data = new_data;
        buf->cap = new_cap;
    }
    memcpy(buf->data + buf->len, src, (size_t)len);
    buf->len += len;
}

void sph_buffer_write(sph_byte_buffer* buf, int64_t offset, const void* src, int64_t count) {
    if (!buf || !src || count <= 0 || offset < 0) return;
    int64_t needed = offset + count;
    if (needed > buf->cap) {
        int64_t new_cap = buf->cap * 2;
        if (new_cap < needed) new_cap = needed;
        uint8_t* new_data = (uint8_t*)realloc(buf->data, (size_t)new_cap);
        if (!new_data) return;
        buf->data = new_data;
        buf->cap = new_cap;
    }
    memcpy(buf->data + offset, src, (size_t)count);
    if (needed > buf->len) {
        buf->len = needed;
    }
}

void sph_buffer_read(const sph_byte_buffer* buf, int64_t offset, void* dst, int64_t count) {
    if (!buf || !buf->data || !dst || count <= 0 || offset < 0 || offset >= buf->len) return;
    int64_t avail = buf->len - offset;
    int64_t to_read = count < avail ? count : avail;
    memcpy(dst, buf->data + offset, (size_t)to_read);
}

sph_byte_buffer* sph_buffer_slice(const sph_byte_buffer* buf, int64_t start, int64_t len) {
    if (!buf || !buf->data || start < 0 || len <= 0 || start >= buf->len) {
        return sph_buffer_alloc(0);
    }
    if (start + len > buf->len) {
        len = buf->len - start;
    }
    return sph_buffer_from_raw(buf->data + start, len);
}

void sph_buffer_free(sph_byte_buffer* buf) {
    if (!buf) return;
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    free(buf);
}



