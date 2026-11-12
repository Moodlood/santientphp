#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void sph_println_i32(int32_t val);
void sph_println_i64(int64_t val);
void sph_println_f32(float val);
void sph_println_f64(double val);
void sph_println_bool(int32_t val);
void sph_println_string(const char* val);

void* sph_alloc(int64_t size);
void sph_free(void* ptr);

typedef struct {
    int64_t ref_count;
    int64_t _reserved;
} sph_rc_header;

void* sph_rc_alloc(int64_t size);
void sph_rc_retain(void* ptr);
void sph_rc_release(void* ptr);
int64_t sph_rc_count(void* ptr);

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

// Native String Operations
int64_t sph_string_len(const char* s);
char* sph_string_concat(const char* a, const char* b);
int32_t sph_string_eq(const char* a, const char* b);
char* sph_string_slice(const char* s, int64_t start, int64_t len);

// Dynamic Vector Collection (Vec<T>)
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

// Standard I/O, Filesystem & OS Process APIs
char* sph_file_read(const char* path);
int32_t sph_file_write(const char* path, const char* content);
int32_t sph_file_append(const char* path, const char* content);
int32_t sph_file_exists(const char* path);
int32_t sph_file_delete(const char* path);
void sph_process_exit(int32_t code);
char* sph_env_get(const char* name);
int32_t sph_env_set(const char* name, const char* value);

// Panic & Error Handling Primitives (Result<T, E>)
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

// PHP Compatibility Subsystem: Dynamic Values (sph_value)
typedef enum {
    SPH_TYPE_NULL = 0,
    SPH_TYPE_BOOL = 1,
    SPH_TYPE_INT = 2,
    SPH_TYPE_FLOAT = 3,
    SPH_TYPE_STRING = 4,
    SPH_TYPE_ARRAY = 5,
    SPH_TYPE_OBJECT = 6
} sph_value_type;

typedef struct sph_value {
    int32_t type;
    int32_t _reserved;
    union {
        int32_t b;
        int64_t i;
        double f;
        char* s;
        void* a;
    } data;
} sph_value;

void* sph_value_create_null(void);
void* sph_value_create_bool(int32_t val);
void* sph_value_create_int(int64_t val);
void* sph_value_create_float(double val);
void* sph_value_create_string(const char* val);
void* sph_value_create_array(void* arr);
void* sph_value_create_array_empty(void);
void* sph_value_create_object(const char* class_name, void* native_instance);
void* sph_object_create(const char* class_name, void* native_instance);
void sph_object_destroy(void* obj_ptr);
void* sph_object_get_property(void* obj_or_val, const char* name);
void sph_object_set_property(void* obj_or_val, const char* name, void* val_ptr);
const char* sph_object_get_class(void* obj_or_val);
void* sph_object_get_native(void* obj_or_val);
int32_t sph_is_object(void* val);
int32_t sph_property_exists(void* obj_or_val, const char* name);
void sph_value_destroy(void* val_ptr);
int32_t sph_value_get_type(void* val_ptr);

void* sph_array_create(void);
void* sph_array_get(void* arr_or_val, void* key_val);
void sph_array_set(void* arr_or_val, void* key_val, void* elem_val);
void sph_array_append(void* arr_or_val, void* elem_val);
int64_t sph_array_count(void* arr_or_val);
void sph_array_destroy(void* arr_ptr);

void* sph_array_iter_create(void* arr_or_val);
int32_t sph_array_iter_valid(void* iter);
void* sph_array_iter_get_key(void* iter);
void* sph_array_iter_get_val(void* iter);
void* sph_array_iter_next(void* iter);

int32_t sph_is_array(void* val);
int32_t sph_is_string(void* val);
int32_t sph_is_int(void* val);
int32_t sph_is_float(void* val);
int32_t sph_is_bool(void* val);
int32_t sph_is_null(void* val);

int64_t sph_array_push(void* arr_or_val, void* elem_val);
void* sph_array_pop(void* arr_or_val);
void* sph_array_shift(void* arr_or_val);
void* sph_array_keys(void* arr_or_val);
void* sph_array_values(void* arr_or_val);
int32_t sph_in_array(void* needle_val, void* arr_or_val);
int32_t sph_array_key_exists(void* key_val, void* arr_or_val);

int64_t sph_php_strlen(void* val_or_str);
char* sph_php_substr(void* val_or_str, int64_t start, int64_t length);
int64_t sph_php_strpos(void* haystack_val, void* needle_val);
void* sph_php_explode(void* delim_val, void* str_val);
char* sph_php_implode(void* glue_val, void* arr_val);

int32_t sph_value_to_bool(void* val_ptr);
int64_t sph_value_to_int(void* val_ptr);
double sph_value_to_float(void* val_ptr);
char* sph_value_to_string(void* val_ptr);

void* sph_dynamic_add(void* a_ptr, void* b_ptr);
void* sph_dynamic_sub(void* a_ptr, void* b_ptr);
void* sph_dynamic_mul(void* a_ptr, void* b_ptr);
void* sph_dynamic_div(void* a_ptr, void* b_ptr);
void* sph_dynamic_mod(void* a_ptr, void* b_ptr);
void* sph_dynamic_concat(void* a_ptr, void* b_ptr);

int32_t sph_dynamic_eq(void* a_ptr, void* b_ptr);
int32_t sph_dynamic_ne(void* a_ptr, void* b_ptr);
int32_t sph_dynamic_lt(void* a_ptr, void* b_ptr);
int32_t sph_dynamic_le(void* a_ptr, void* b_ptr);
int32_t sph_dynamic_gt(void* a_ptr, void* b_ptr);
int32_t sph_dynamic_ge(void* a_ptr, void* b_ptr);

void sph_echo_value(void* val_ptr);
void sph_echo_string(const char* str);
void sph_echo_i32(int32_t val);
void sph_echo_i64(int64_t val);
void sph_echo_f64(double val);
void sph_echo_bool(int32_t val);
void sph_var_dump(void* val_ptr);

// Exception Unwinding & Frame Management
#include <setjmp.h>

typedef struct sph_exception_frame {
    char env[256];
    void* caught_exception;
    struct sph_exception_frame* prev;
} sph_exception_frame;

int sph_setjmp(void* env);
void sph_longjmp(void* env, int val);
void sph_exception_push_frame(sph_exception_frame* frame);
void sph_exception_pop_frame(void);
void sph_exception_throw(void* exception_obj);
void* sph_exception_get_current(void);
int64_t sph_exception_frame_size(void);

// ============================================================================
// Database Interoperability (PDO & SQLite)
// ============================================================================

#define SPH_PDO_FETCH_ASSOC 2
#define SPH_PDO_FETCH_NUM   3
#define SPH_PDO_FETCH_BOTH  4

typedef struct sph_pdo sph_pdo;
typedef struct sph_pdo_statement sph_pdo_statement;

void* sph_pdo_open(const char* dsn, const char* username, const char* password);
void sph_pdo_close(void* pdo_ptr);
int64_t sph_pdo_exec(void* pdo_ptr, const char* sql);
void* sph_pdo_query(void* pdo_ptr, const char* sql);
void* sph_pdo_prepare(void* pdo_ptr, const char* sql);
int64_t sph_pdo_last_insert_id(void* pdo_ptr);

int32_t sph_pdostatement_execute(void* stmt_ptr, void* params_arr_or_val);
void* sph_pdostatement_fetch(void* stmt_ptr, int32_t mode);
void* sph_pdostatement_fetch_all(void* stmt_ptr, int32_t mode);
int64_t sph_pdostatement_row_count(void* stmt_ptr);
int32_t sph_pdostatement_column_count(void* stmt_ptr);
void sph_pdostatement_close(void* stmt_ptr);

// ============================================================================
// HTTP Interoperability (cURL & WinHTTP)
// ============================================================================

#define SPH_CURLOPT_URL             10002
#define SPH_CURLOPT_RETURNTRANSFER  19913
#define SPH_CURLOPT_POST            47
#define SPH_CURLOPT_POSTFIELDS      10015
#define SPH_CURLOPT_HTTPHEADER      10023
#define SPH_CURLOPT_TIMEOUT         13
#define SPH_CURLOPT_CUSTOMREQUEST   10036
#define SPH_CURLINFO_HTTP_CODE      2097154

void* sph_curl_init(const char* url);
int32_t sph_curl_setopt(void* ch, int32_t opt, void* val_ptr);
void* sph_curl_exec(void* ch);
void* sph_curl_getinfo(void* ch, int32_t opt);
void sph_curl_close(void* ch);
char* sph_http_get(const char* url);

// ============================================================================
// Concurrency, Threading, and Atomics (Phase 11 Milestone 11A)
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

// Native Threading via Windows Kernel32
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

// =========================================================================
// Milestone 11C: SIMD (Single Instruction, Multiple Data) & CPU Feature Detection
// =========================================================================

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

// Target CPU Feature Detection Subsystem (Simd::has_*)
int32_t sph_simd_has_sse(void);
int32_t sph_simd_has_sse2(void);
int32_t sph_simd_has_sse3(void);
int32_t sph_simd_has_sse41(void);
int32_t sph_simd_has_avx(void);
int32_t sph_simd_has_avx2(void);
int32_t sph_simd_has_neon(void);

// f32x4 Vector Operations
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

// i32x4 Vector Operations
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

// f64x2 Vector Operations
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

// i64x2 Vector Operations
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

