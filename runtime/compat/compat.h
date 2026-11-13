#pragma once

#include <stdint.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PHP Compatibility Subsystem: Dynamic Values (sph_value)
// ============================================================================

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

#ifdef __cplusplus
}
#endif

