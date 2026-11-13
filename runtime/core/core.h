#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Core Primitive Printing
// ============================================================================

void sph_println_i32(int32_t val);
void sph_println_i64(int64_t val);
void sph_println_f32(float val);
void sph_println_f64(double val);
void sph_println_bool(int32_t val);
void sph_println_string(const char* val);

// ============================================================================
// Raw Memory Allocator
// ============================================================================

void* sph_alloc(int64_t size);
void sph_free(void* ptr);

// ============================================================================
// Reference Counted Smart Pointers (Rc<T>)
// ============================================================================

typedef struct {
    int64_t ref_count;
    int64_t _reserved;
} sph_rc_header;

void* sph_rc_alloc(int64_t size);
void sph_rc_retain(void* ptr);
void sph_rc_release(void* ptr);
int64_t sph_rc_count(void* ptr);

// ============================================================================
// Arena Region Allocator
// ============================================================================

typedef struct sph_arena_chunk {
    struct sph_arena_chunk* next;
    int64_t capacity;
    int64_t offset;
} sph_arena_chunk;

typedef struct {
    sph_arena_chunk* first;
    sph_arena_chunk* current;
    int64_t default_chunk_size;
} sph_arena;

void* sph_arena_create(int64_t default_chunk_size);
void* sph_arena_alloc(void* arena_ptr, int64_t size, int64_t align);
void sph_arena_reset(void* arena_ptr);
void sph_arena_destroy(void* arena_ptr);

// ============================================================================
// Native String Operations
// ============================================================================

int64_t sph_string_len(const char* s);
char* sph_string_concat(const char* a, const char* b);
int32_t sph_string_eq(const char* a, const char* b);
char* sph_string_slice(const char* s, int64_t start, int64_t len);

// ============================================================================
// Dynamic Vector Collection (Vec<T>)
// ============================================================================

typedef struct {
    void* data;
    int64_t len;
    int64_t cap;
    int64_t elem_size;
} sph_vec;

void* sph_vec_create(int64_t elem_size, int64_t initial_cap);
void sph_vec_push(void* vec_ptr, const void* elem_ptr);
void sph_vec_pop(void* vec_ptr, void* out_elem);
void* sph_vec_get(void* vec_ptr, int64_t index);
void sph_vec_set(void* vec_ptr, int64_t index, const void* elem_ptr);
int64_t sph_vec_len(void* vec_ptr);
int64_t sph_vec_cap(void* vec_ptr);
void sph_vec_destroy(void* vec_ptr);

// ============================================================================
// Standard I/O, Filesystem & OS Process APIs
// ============================================================================

typedef char* (*sph_file_read_http_fn)(const char* url);
void sph_register_http_get_hook(sph_file_read_http_fn hook);

char* sph_file_read(const char* path);
int32_t sph_file_write(const char* path, const char* content);
int32_t sph_file_append(const char* path, const char* content);
int32_t sph_file_exists(const char* path);
int32_t sph_file_delete(const char* path);
void sph_process_exit(int32_t code);
char* sph_env_get(const char* name);
int32_t sph_env_set(const char* name, const char* value);

// ============================================================================
// Panic & Error Handling Primitives (Result<T, E>)
// ============================================================================

void sph_panic(const char* msg);

typedef struct {
    int32_t is_err;
    int32_t _reserved;
    char payload[];
} sph_result;

void* sph_result_create_ok(int64_t size, const void* val_ptr);
void* sph_result_create_err(int64_t size, const void* err_ptr);
int32_t sph_result_is_ok(void* res_ptr);
int32_t sph_result_is_err(void* res_ptr);
void* sph_result_unwrap(void* res_ptr);
void sph_result_destroy(void* res_ptr);

// ============================================================================
// SIMD (Single Instruction, Multiple Data) & CPU Feature Detection
// ============================================================================

typedef union sph_f32x4 {
    float f[4];
    struct { float x, y, z, w; };
} sph_f32x4;

typedef union sph_i32x4 {
    int32_t i[4];
    struct { int32_t x, y, z, w; };
} sph_i32x4;

typedef union sph_f64x2 {
    double d[2];
    struct { double x, y; };
} sph_f64x2;

typedef union sph_i64x2 {
    int64_t l[2];
    struct { int64_t x, y; };
} sph_i64x2;

// Feature Detection
int32_t sph_simd_has_sse(void);
int32_t sph_simd_has_sse2(void);
int32_t sph_simd_has_sse3(void);
int32_t sph_simd_has_sse41(void);
int32_t sph_simd_has_avx(void);
int32_t sph_simd_has_avx2(void);
int32_t sph_simd_has_neon(void);

// f32x4
void sph_simd_f32x4_construct(sph_f32x4* out, float x, float y, float z, float w);
void sph_simd_f32x4_splat(sph_f32x4* out, float val);
void sph_simd_f32x4_zero(sph_f32x4* out);
void sph_simd_f32x4_load(sph_f32x4* out, const float* ptr);
void sph_simd_f32x4_store(float* ptr, const sph_f32x4* vec);
float sph_simd_f32x4_get(const sph_f32x4* vec, int32_t index);
void sph_simd_f32x4_set(sph_f32x4* vec, int32_t index, float val);
void sph_simd_f32x4_add(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b);
void sph_simd_f32x4_sub(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b);
void sph_simd_f32x4_mul(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b);
void sph_simd_f32x4_div(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b);
void sph_simd_f32x4_sqrt(sph_f32x4* out, const sph_f32x4* vec);
void sph_simd_f32x4_abs(sph_f32x4* out, const sph_f32x4* vec);
void sph_simd_f32x4_min(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b);
void sph_simd_f32x4_max(sph_f32x4* out, const sph_f32x4* a, const sph_f32x4* b);
float sph_simd_f32x4_sum(const sph_f32x4* vec);
float sph_simd_f32x4_dot(const sph_f32x4* a, const sph_f32x4* b);
char* sph_simd_f32x4_to_string(const sph_f32x4* vec);
void sph_println_f32x4(const sph_f32x4* vec);

// i32x4
void sph_simd_i32x4_construct(sph_i32x4* out, int32_t x, int32_t y, int32_t z, int32_t w);
void sph_simd_i32x4_splat(sph_i32x4* out, int32_t val);
void sph_simd_i32x4_zero(sph_i32x4* out);
void sph_simd_i32x4_load(sph_i32x4* out, const int32_t* ptr);
void sph_simd_i32x4_store(int32_t* ptr, const sph_i32x4* vec);
int32_t sph_simd_i32x4_get(const sph_i32x4* vec, int32_t index);
void sph_simd_i32x4_set(sph_i32x4* vec, int32_t index, int32_t val);
void sph_simd_i32x4_add(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b);
void sph_simd_i32x4_sub(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b);
void sph_simd_i32x4_mul(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b);
void sph_simd_i32x4_div(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b);
void sph_simd_i32x4_abs(sph_i32x4* out, const sph_i32x4* vec);
void sph_simd_i32x4_min(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b);
void sph_simd_i32x4_max(sph_i32x4* out, const sph_i32x4* a, const sph_i32x4* b);
int32_t sph_simd_i32x4_sum(const sph_i32x4* vec);
int32_t sph_simd_i32x4_dot(const sph_i32x4* a, const sph_i32x4* b);
char* sph_simd_i32x4_to_string(const sph_i32x4* vec);
void sph_println_i32x4(const sph_i32x4* vec);

// f64x2
void sph_simd_f64x2_construct(sph_f64x2* out, double x, double y);
void sph_simd_f64x2_splat(sph_f64x2* out, double val);
void sph_simd_f64x2_zero(sph_f64x2* out);
void sph_simd_f64x2_load(sph_f64x2* out, const double* ptr);
void sph_simd_f64x2_store(double* ptr, const sph_f64x2* vec);
double sph_simd_f64x2_get(const sph_f64x2* vec, int32_t index);
void sph_simd_f64x2_set(sph_f64x2* vec, int32_t index, double val);
void sph_simd_f64x2_add(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b);
void sph_simd_f64x2_sub(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b);
void sph_simd_f64x2_mul(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b);
void sph_simd_f64x2_div(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b);
void sph_simd_f64x2_sqrt(sph_f64x2* out, const sph_f64x2* vec);
void sph_simd_f64x2_abs(sph_f64x2* out, const sph_f64x2* vec);
void sph_simd_f64x2_min(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b);
void sph_simd_f64x2_max(sph_f64x2* out, const sph_f64x2* a, const sph_f64x2* b);
double sph_simd_f64x2_sum(const sph_f64x2* vec);
double sph_simd_f64x2_dot(const sph_f64x2* a, const sph_f64x2* b);
char* sph_simd_f64x2_to_string(const sph_f64x2* vec);
void sph_println_f64x2(const sph_f64x2* vec);

// i64x2
void sph_simd_i64x2_construct(sph_i64x2* out, int64_t x, int64_t y);
void sph_simd_i64x2_splat(sph_i64x2* out, int64_t val);
void sph_simd_i64x2_zero(sph_i64x2* out);
void sph_simd_i64x2_load(sph_i64x2* out, const int64_t* ptr);
void sph_simd_i64x2_store(int64_t* ptr, const sph_i64x2* vec);
int64_t sph_simd_i64x2_get(const sph_i64x2* vec, int32_t index);
void sph_simd_i64x2_set(sph_i64x2* vec, int32_t index, int64_t val);
void sph_simd_i64x2_add(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b);
void sph_simd_i64x2_sub(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b);
void sph_simd_i64x2_mul(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b);
void sph_simd_i64x2_div(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b);
void sph_simd_i64x2_abs(sph_i64x2* out, const sph_i64x2* vec);
void sph_simd_i64x2_min(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b);
void sph_simd_i64x2_max(sph_i64x2* out, const sph_i64x2* a, const sph_i64x2* b);
int64_t sph_simd_i64x2_sum(const sph_i64x2* vec);
int64_t sph_simd_i64x2_dot(const sph_i64x2* a, const sph_i64x2* b);
char* sph_simd_i64x2_to_string(const sph_i64x2* vec);
void sph_println_i64x2(const sph_i64x2* vec);

#ifdef __cplusplus
}
#endif
