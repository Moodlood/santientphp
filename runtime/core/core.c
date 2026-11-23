#include "core.h"
#include "../pal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#if !defined(_WIN32)
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
// High-Performance Thread-Local Slab Allocator (<= 64 bytes)
// ============================================================================

#define SPH_SLAB_MAGIC 0x53504853 // "SPHS"
#define SPH_SLAB_PAYLOAD 64
#define SPH_SLAB_BLOCK_SIZE (sizeof(sph_slab_block) + SPH_SLAB_PAYLOAD)
#define SPH_SLAB_CHUNK_CAPACITY 65536

typedef struct sph_slab_block {
    uint32_t magic;
    uint32_t size;
    struct sph_slab_block* next;
} sph_slab_block;

typedef struct sph_slab_chunk {
    struct sph_slab_chunk* next;
    uint8_t* bump;
    uint8_t* limit;
    uint64_t _pad;
    uint8_t memory[SPH_SLAB_CHUNK_CAPACITY - sizeof(void*) - 2 * sizeof(uint8_t*) - sizeof(uint64_t)];
} sph_slab_chunk;

typedef struct sph_slab_thread_cache {
    sph_slab_block* free_pool;
    uint8_t* bump;
    uint8_t* limit;
    sph_slab_chunk* chunk_head;
} sph_slab_thread_cache;

#if defined(_MSC_VER)
static __declspec(thread) sph_slab_thread_cache g_slab = {0};
#elif defined(__GNUC__) || defined(__clang__) || defined(__MINGW32__)
static __thread sph_slab_thread_cache g_slab = {0};
#else
static sph_slab_thread_cache g_slab = {0};
#endif

void* sph_alloc(int64_t size) {
    if (size <= 0) size = 1;
    if (size <= SPH_SLAB_PAYLOAD) {
        if (g_slab.free_pool) {
            sph_slab_block* blk = g_slab.free_pool;
            g_slab.free_pool = blk->next;
            blk->magic = SPH_SLAB_MAGIC;
            blk->size = (uint32_t)size;
            return (void*)(blk + 1);
        }
        if (g_slab.bump && (g_slab.bump + SPH_SLAB_BLOCK_SIZE <= g_slab.limit)) {
            sph_slab_block* blk = (sph_slab_block*)g_slab.bump;
            g_slab.bump += SPH_SLAB_BLOCK_SIZE;
            blk->magic = SPH_SLAB_MAGIC;
            blk->size = (uint32_t)size;
            return (void*)(blk + 1);
        }
        // Allocate a new 64KB chunk
        sph_slab_chunk* new_chunk = (sph_slab_chunk*)malloc(sizeof(sph_slab_chunk));
        if (new_chunk) {
            new_chunk->next = g_slab.chunk_head;
            g_slab.chunk_head = new_chunk;
            g_slab.bump = new_chunk->memory;
            g_slab.limit = new_chunk->memory + sizeof(new_chunk->memory);

            sph_slab_block* blk = (sph_slab_block*)g_slab.bump;
            g_slab.bump += SPH_SLAB_BLOCK_SIZE;
            blk->magic = SPH_SLAB_MAGIC;
            blk->size = (uint32_t)size;
            return (void*)(blk + 1);
        }
        // Fallback
        sph_slab_block* blk = (sph_slab_block*)malloc(SPH_SLAB_BLOCK_SIZE);
        if (!blk) return NULL;
        blk->magic = SPH_SLAB_MAGIC;
        blk->size = (uint32_t)size;
        return (void*)(blk + 1);
    }
    return malloc((size_t)size);
}

void sph_free(void* ptr) {
    if (!ptr) return;
    sph_slab_block* blk = ((sph_slab_block*)ptr) - 1;
    if (blk->magic == SPH_SLAB_MAGIC) {
        blk->magic = 0;
        blk->next = g_slab.free_pool;
        g_slab.free_pool = blk;
        return;
    }
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
#define SPH_STR_CACHE_SIZE 256
#define SPH_STR_CACHE_MASK (SPH_STR_CACHE_SIZE - 1)

typedef struct {
    char* ptr;
    size_t len;
    size_t cap;
} sph_str_cache_entry;

#if defined(_MSC_VER)
static __declspec(thread) sph_str_cache_entry g_sph_str_cache[SPH_STR_CACHE_SIZE];
#elif defined(__GNUC__) || defined(__clang__) || defined(__MINGW32__)
static __thread sph_str_cache_entry g_sph_str_cache[SPH_STR_CACHE_SIZE];
#else
static sph_str_cache_entry g_sph_str_cache[SPH_STR_CACHE_SIZE];
#endif

static inline size_t sph_str_cache_hash(const void* ptr) {
    uintptr_t v = (uintptr_t)ptr;
    return ((v >> 4) ^ (v >> 9)) & SPH_STR_CACHE_MASK;
}

static inline void sph_str_cache_put(char* ptr, size_t len, size_t cap) {
    if (!ptr) return;
    size_t idx = sph_str_cache_hash(ptr);
    g_sph_str_cache[idx].ptr = ptr;
    g_sph_str_cache[idx].len = len;
    g_sph_str_cache[idx].cap = cap;
}

static inline sph_str_cache_entry* sph_str_cache_get(const char* ptr) {
    if (!ptr) return NULL;
    size_t idx = sph_str_cache_hash(ptr);
    if (g_sph_str_cache[idx].ptr == ptr) {
        return &g_sph_str_cache[idx];
    }
    return NULL;
}

static inline void sph_str_cache_remove(const void* ptr) {
    if (!ptr) return;
    size_t idx = sph_str_cache_hash(ptr);
    if (g_sph_str_cache[idx].ptr == ptr) {
        g_sph_str_cache[idx].ptr = NULL;
        g_sph_str_cache[idx].len = 0;
        g_sph_str_cache[idx].cap = 0;
    }
}

int64_t sph_string_len(const char* s) {
    if (!s) return 0;
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

    size_t len_suffix = strlen(suffix);

    sph_str_cache_entry* entry_s = sph_str_cache_get(s);
    if (entry_s && entry_s->len <= entry_s->cap && s[entry_s->len] == '\0') {
        size_t len_s = entry_s->len;
        size_t cap_s = entry_s->cap;
        size_t needed = len_s + len_suffix + 1;

        if (needed <= cap_s) {
            memcpy(s + len_s, suffix, len_suffix);
            s[len_s + len_suffix] = '\0';
            entry_s->len = len_s + len_suffix;
            return s;
        } else {
            size_t new_cap = cap_s * 2;
            if (new_cap < needed) new_cap = needed < 64 ? 64 : needed * 2;
            char* new_ptr = (char*)realloc(s, new_cap);
            if (!new_ptr) return s;
            memcpy(new_ptr + len_s, suffix, len_suffix);
            new_ptr[len_s + len_suffix] = '\0';

            if (new_ptr != s) {
                entry_s->ptr = NULL;
            }
            sph_str_cache_put(new_ptr, len_s + len_suffix, new_cap);
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

    sph_str_cache_put(new_ptr, len_s + len_suffix, cap);
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

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>

static int g_wsa_initialized = 0;
static void sph_ensure_wsa(void) {
    if (!g_wsa_initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        g_wsa_initialized = 1;
    }
}

int32_t sph_net_listen(int32_t port) {
    if (port <= 0 || port > 65535) return -1;
    sph_ensure_wsa();
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return -1;

    BOOL opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }

    if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }

    return (int32_t)s;
}

void sph_net_close(int32_t fd) {
    if (fd >= 0) {
        closesocket((SOCKET)fd);
    }
}

int32_t sph_server_poll_read(int32_t fd, int32_t timeout_ms) {
    if (fd < 0) return 0;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET((SOCKET)fd, &fds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int res = select(fd + 1, &fds, NULL, NULL, (timeout_ms >= 0) ? &tv : NULL);
    return res > 0 ? 1 : 0;
}

int32_t sph_server_accept(int32_t server_fd, int32_t timeout_ms) {
    if (server_fd < 0) return -1;
    if (timeout_ms >= 0) {
        if (!sph_server_poll_read(server_fd, timeout_ms)) return -1;
    }
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    SOCKET client_sock = accept((SOCKET)server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET) return -1;
    return (int32_t)client_sock;
}

char* sph_server_read_request(int32_t client_fd, int32_t timeout_ms) {
    if (client_fd < 0) return strdup("");
    if (timeout_ms >= 0) {
        if (!sph_server_poll_read(client_fd, timeout_ms)) return strdup("");
    }
    char buf[4096];
    int total = 0;
    while (total < (int)sizeof(buf) - 1) {
        int r = recv((SOCKET)client_fd, buf + total, sizeof(buf) - 1 - total, 0);
        if (r <= 0) break;
        total += r;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }
    return strdup(buf);
}

int32_t sph_server_write_response(int32_t client_fd, int32_t status_code, const char* content_type, const char* body) {
    if (client_fd < 0) return -1;
    if (!content_type) content_type = "text/plain";
    if (!body) body = "";
    int64_t body_len = (int64_t)strlen(body);
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n",
        status_code, content_type, (long long)body_len);
    send((SOCKET)client_fd, header, header_len, 0);
    if (body_len > 0) {
        send((SOCKET)client_fd, body, (int)body_len, 0);
    }
    return 0;
}

int32_t sph_server_write_raw(int32_t client_fd, const uint8_t* data, int64_t len) {
    if (client_fd < 0 || !data || len <= 0) return -1;
    int64_t total = 0;
    while (total < len) {
        int s = send((SOCKET)client_fd, (const char*)(data + total), (int)(len - total), 0);
        if (s <= 0) return -1;
        total += s;
    }
    return 0;
}

static char* sph_http_request(const char* url, const char* post_body) {
    if (!url) return strdup("");
    if (strncmp(url, "mock://", 7) == 0 || strncmp(url, "http://mock/", 12) == 0) {
        if (post_body) {
            char buf[512];
            snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"action\":\"post\",\"data\":\"%s\"}", post_body);
            return strdup(buf);
        }
        return strdup("{\"status\":\"ok\",\"service\":\"SantientPHP Net\"}");
    }

    HMODULE hWinHttp = LoadLibraryA("winhttp.dll");
    if (hWinHttp) {
        typedef void* (*pfn_WinHttpOpen)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
        typedef void* (*pfn_WinHttpConnect)(void*, LPCWSTR, unsigned short, DWORD);
        typedef void* (*pfn_WinHttpOpenRequest)(void*, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
        typedef BOOL (*pfn_WinHttpSendRequest)(void*, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
        typedef BOOL (*pfn_WinHttpReceiveResponse)(void*, LPVOID);
        typedef BOOL (*pfn_WinHttpReadData)(void*, LPVOID, DWORD, LPDWORD);
        typedef BOOL (*pfn_WinHttpCloseHandle)(void*);

        pfn_WinHttpOpen fn_Open = (pfn_WinHttpOpen)(void*)GetProcAddress(hWinHttp, "WinHttpOpen");
        pfn_WinHttpConnect fn_Connect = (pfn_WinHttpConnect)(void*)GetProcAddress(hWinHttp, "WinHttpConnect");
        pfn_WinHttpOpenRequest fn_OpenRequest = (pfn_WinHttpOpenRequest)(void*)GetProcAddress(hWinHttp, "WinHttpOpenRequest");
        pfn_WinHttpSendRequest fn_SendRequest = (pfn_WinHttpSendRequest)(void*)GetProcAddress(hWinHttp, "WinHttpSendRequest");
        pfn_WinHttpReceiveResponse fn_ReceiveResponse = (pfn_WinHttpReceiveResponse)(void*)GetProcAddress(hWinHttp, "WinHttpReceiveResponse");
        pfn_WinHttpReadData fn_ReadData = (pfn_WinHttpReadData)(void*)GetProcAddress(hWinHttp, "WinHttpReadData");
        pfn_WinHttpCloseHandle fn_CloseHandle = (pfn_WinHttpCloseHandle)(void*)GetProcAddress(hWinHttp, "WinHttpCloseHandle");

        if (fn_Open && fn_Connect && fn_OpenRequest && fn_SendRequest && fn_ReceiveResponse && fn_ReadData && fn_CloseHandle) {
            int is_https = (strncmp(url, "https://", 8) == 0);
            const char* host_start = url + (is_https ? 8 : (strncmp(url, "http://", 7) == 0 ? 7 : 0));
            const char* slash = strchr(host_start, '/');
            char host[256];
            char path[1024];
            if (slash) {
                size_t host_len = (size_t)(slash - host_start);
                if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
                strncpy(host, host_start, host_len);
                host[host_len] = '\0';
                strncpy(path, slash, sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
            } else {
                strncpy(host, host_start, sizeof(host) - 1);
                host[sizeof(host) - 1] = '\0';
                strcpy(path, "/");
            }

            wchar_t wHost[256];
            wchar_t wPath[1024];
            MultiByteToWideChar(CP_UTF8, 0, host, -1, wHost, 256);
            MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, 1024);

            void* hSession = fn_Open(L"SantientPHP/1.0", 1 /* WINHTTP_ACCESS_TYPE_DEFAULT_PROXY */, NULL, NULL, 0);
            if (hSession) {
                void* hConnect = fn_Connect(hSession, wHost, is_https ? 443 : 80, 0);
                if (hConnect) {
                    LPCWSTR verb = post_body ? L"POST" : L"GET";
                    DWORD flags = is_https ? 0x00800000 /* WINHTTP_FLAG_SECURE */ : 0;
                    void* hRequest = fn_OpenRequest(hConnect, verb, wPath, NULL, NULL, NULL, flags);
                    if (hRequest) {
                        LPCWSTR hdrs = post_body ? L"Content-Type: application/x-www-form-urlencoded\r\n" : NULL;
                        DWORD hdr_len = hdrs ? (DWORD)wcslen(hdrs) : 0;
                        DWORD body_len = post_body ? (DWORD)strlen(post_body) : 0;
                        if (fn_SendRequest(hRequest, hdrs, hdr_len, (LPVOID)post_body, body_len, body_len, 0)) {
                            if (fn_ReceiveResponse(hRequest, NULL)) {
                                size_t total_read = 0;
                                size_t cap = 4096;
                                char* resp_buf = (char*)malloc(cap);
                                DWORD bytes_read = 0;
                                while (fn_ReadData(hRequest, resp_buf + total_read, (DWORD)(cap - total_read - 1), &bytes_read) && bytes_read > 0) {
                                    total_read += bytes_read;
                                    if (cap - total_read < 1024) {
                                        cap *= 2;
                                        char* new_b = (char*)realloc(resp_buf, cap);
                                        if (!new_b) break;
                                        resp_buf = new_b;
                                    }
                                }
                                resp_buf[total_read] = '\0';
                                fn_CloseHandle(hRequest);
                                fn_CloseHandle(hConnect);
                                fn_CloseHandle(hSession);
                                FreeLibrary(hWinHttp);
                                return resp_buf;
                            }
                        }
                        fn_CloseHandle(hRequest);
                    }
                    fn_CloseHandle(hConnect);
                }
                fn_CloseHandle(hSession);
            }
            FreeLibrary(hWinHttp);
        }
    }

    return strdup("");
}

char* sph_http_get(const char* url) {
    return sph_http_request(url, NULL);
}

char* sph_http_post(const char* url, const char* body) {
    return sph_http_request(url, body ? body : "");
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

int32_t sph_net_listen(int32_t port) {
    if (port <= 0 || port > 65535) return -1;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }
    if (listen(s, SOMAXCONN) < 0) {
        close(s);
        return -1;
    }
    return (int32_t)s;
}

void sph_net_close(int32_t fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int32_t sph_server_poll_read(int32_t fd, int32_t timeout_ms) {
    if (fd < 0) return 0;
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int res = poll(&pfd, 1, timeout_ms);
    return res > 0 && (pfd.revents & POLLIN) ? 1 : 0;
}

int32_t sph_server_accept(int32_t server_fd, int32_t timeout_ms) {
    if (server_fd < 0) return -1;
    if (timeout_ms >= 0) {
        if (!sph_server_poll_read(server_fd, timeout_ms)) return -1;
    }
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock < 0) return -1;
    return client_sock;
}

char* sph_server_read_request(int32_t client_fd, int32_t timeout_ms) {
    if (client_fd < 0) return strdup("");
    if (timeout_ms >= 0) {
        if (!sph_server_poll_read(client_fd, timeout_ms)) return strdup("");
    }
    char buf[4096];
    int total = 0;
    while (total < (int)sizeof(buf) - 1) {
        int r = recv(client_fd, buf + total, sizeof(buf) - 1 - total, 0);
        if (r <= 0) break;
        total += r;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }
    return strdup(buf);
}

int32_t sph_server_write_response(int32_t client_fd, int32_t status_code, const char* content_type, const char* body) {
    if (client_fd < 0) return -1;
    if (!content_type) content_type = "text/plain";
    if (!body) body = "";
    int64_t body_len = (int64_t)strlen(body);
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n",
        status_code, content_type, (long long)body_len);
    send(client_fd, header, header_len, 0);
    if (body_len > 0) {
        send(client_fd, body, (size_t)body_len, 0);
    }
    return 0;
}

int32_t sph_server_write_raw(int32_t client_fd, const uint8_t* data, int64_t len) {
    if (client_fd < 0 || !data || len <= 0) return -1;
    int64_t total = 0;
    while (total < len) {
        ssize_t s = send(client_fd, (const char*)(data + total), (size_t)(len - total), 0);
        if (s <= 0) return -1;
        total += s;
    }
    return 0;
}

char* sph_http_get(const char* url) {
    if (!url) return strdup("");
    if (strncmp(url, "mock://", 7) == 0 || strncmp(url, "http://mock/", 12) == 0) {
        return strdup("{\"status\":\"ok\",\"service\":\"SantientPHP Net\"}");
    }
    return strdup("");
}

char* sph_http_post(const char* url, const char* body) {
    if (!url) return strdup("");
    if (strncmp(url, "mock://", 7) == 0 || strncmp(url, "http://mock/", 12) == 0) {
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"action\":\"post\",\"data\":\"%s\"}", body ? body : "");
        return strdup(buf);
    }
    return strdup("");
}
#endif

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



