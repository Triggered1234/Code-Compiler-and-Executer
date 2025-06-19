/**
 * @file common.h
 * @brief Common definitions and utilities for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define PROJECT_NAME "Code Compiler & Executer"
#define PROJECT_VERSION_MAJOR 1
#define PROJECT_VERSION_MINOR 0
#define PROJECT_VERSION_PATCH 0
#define PROJECT_VERSION_STRING "1.0.0"

/* Build information */
#ifndef BUILD_DATE
#define BUILD_DATE __DATE__
#endif

#ifndef BUILD_TIME
#define BUILD_TIME __TIME__
#endif

/* Platform detection */
#if defined(__linux__)
    #define PLATFORM_LINUX 1
    #define PLATFORM_NAME "Linux"
#elif defined(__APPLE__)
    #define PLATFORM_MACOS 1
    #define PLATFORM_NAME "macOS"
#elif defined(_WIN32)
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_NAME "Windows"
#else
    #define PLATFORM_UNKNOWN 1
    #define PLATFORM_NAME "Unknown"
#endif

/* Compiler detection */
#if defined(__GNUC__)
    #define COMPILER_GCC 1
    #define COMPILER_NAME "GCC"
    #define COMPILER_VERSION __VERSION__
#elif defined(__clang__)
    #define COMPILER_CLANG 1
    #define COMPILER_NAME "Clang"
    #define COMPILER_VERSION __clang_version__
#elif defined(_MSC_VER)
    #define COMPILER_MSVC 1
    #define COMPILER_NAME "MSVC"
    #define COMPILER_VERSION _MSC_VER
#else
    #define COMPILER_UNKNOWN 1
    #define COMPILER_NAME "Unknown"
    #define COMPILER_VERSION "Unknown"
#endif

/* Common constants */
#define KB (1024)
#define MB (1024 * KB)
#define GB (1024 * MB)

#define SECOND_IN_MICROSECONDS 1000000
#define MINUTE_IN_SECONDS 60
#define HOUR_IN_SECONDS (60 * MINUTE_IN_SECONDS)
#define DAY_IN_SECONDS (24 * HOUR_IN_SECONDS)

/* Buffer sizes */
#define SMALL_BUFFER_SIZE 256
#define MEDIUM_BUFFER_SIZE 1024
#define LARGE_BUFFER_SIZE 4096
#define HUGE_BUFFER_SIZE 65536

/* File system constants */
#define MAX_FILENAME_LENGTH 255
#define MAX_PATH_LENGTH 4096
#define MAX_SYMLINK_DEPTH 20

/* Network constants */
#define INET_ADDRSTRLEN 16
#define MAX_HOSTNAME_LENGTH 256
#define MAX_PORT_NUMBER 65535

/* Log levels */
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARNING = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3,
    LOG_LEVEL_TRACE = 4
} log_level_t;

/* Return codes */
typedef enum {
    RESULT_SUCCESS = 0,
    RESULT_ERROR = -1,
    RESULT_INVALID_ARGUMENT = -2,
    RESULT_MEMORY_ERROR = -3,
    RESULT_IO_ERROR = -4,
    RESULT_NETWORK_ERROR = -5,
    RESULT_TIMEOUT = -6,
    RESULT_PERMISSION_DENIED = -7,
    RESULT_NOT_FOUND = -8,
    RESULT_ALREADY_EXISTS = -9,
    RESULT_NOT_SUPPORTED = -10,
    RESULT_QUOTA_EXCEEDED = -11,
    RESULT_INTERNAL_ERROR = -99
} result_code_t;

/* Boolean type for C compatibility */
#ifndef __cplusplus
    #ifndef bool
        #define bool int
        #define true 1
        #define false 0
    #endif
#endif

/* Attribute macros for better code optimization */
#ifdef __GNUC__
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define UNUSED      __attribute__((unused))
    #define PACKED      __attribute__((packed))
    #define ALIGNED(n)  __attribute__((aligned(n)))
    #define NORETURN    __attribute__((noreturn))
    #define PRINTF_FORMAT(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
    #define UNUSED
    #define PACKED
    #define ALIGNED(n)
    #define NORETURN
    #define PRINTF_FORMAT(fmt_idx, arg_idx)
#endif

/* Memory management macros */
#define SAFE_FREE(ptr) do { \
    if ((ptr) != NULL) { \
        free(ptr); \
        (ptr) = NULL; \
    } \
} while(0)

#define SAFE_CLOSE(fd) do { \
    if ((fd) >= 0) { \
        close(fd); \
        (fd) = -1; \
    } \
} while(0)

#define SAFE_FCLOSE(fp) do { \
    if ((fp) != NULL) { \
        fclose(fp); \
        (fp) = NULL; \
    } \
} while(0)

/* Array and string macros */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define STRING_EQUAL(s1, s2) (strcmp((s1), (s2)) == 0)
#define STRING_EMPTY(s) ((s) == NULL || (s)[0] == '\0')

/* Mathematical macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(val, min_val, max_val) MAX(min_val, MIN(val, max_val))
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define SIGN(x) ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))

/* Bit manipulation macros */
#define SET_BIT(value, bit) ((value) |= (1U << (bit)))
#define CLEAR_BIT(value, bit) ((value) &= ~(1U << (bit)))
#define TOGGLE_BIT(value, bit) ((value) ^= (1U << (bit)))
#define CHECK_BIT(value, bit) (((value) >> (bit)) & 1U)

/* Alignment macros */
#define ALIGN_UP(value, alignment) (((value) + (alignment) - 1) & ~((alignment) - 1))
#define ALIGN_DOWN(value, alignment) ((value) & ~((alignment) - 1))
#define IS_ALIGNED(value, alignment) (((value) & ((alignment) - 1)) == 0)

/* Time utilities */
typedef struct {
    struct timespec start;
    struct timespec end;
    bool started;
} timer_t;

void timer_start(timer_t *timer);
double timer_stop(timer_t *timer);
double timer_elapsed(const timer_t *timer);

/* Current time functions */
uint64_t get_current_time_ms(void);
uint64_t get_current_time_us(void);
void get_current_timespec(struct timespec *ts);
char* format_time(time_t timestamp, char *buffer, size_t buffer_size);
char* format_time_iso(time_t timestamp, char *buffer, size_t buffer_size);

/* String utilities */
char* safe_strncpy(char *dest, const char *src, size_t dest_size);
char* safe_strncat(char *dest, const char *src, size_t dest_size);
char* trim_whitespace(char *str);
char* trim_newline(char *str);
bool starts_with(const char *str, const char *prefix);
bool ends_with(const char *str, const char *suffix);
bool contains_substring(const char *str, const char *substr);
int safe_snprintf(char *buffer, size_t buffer_size, const char *format, ...) PRINTF_FORMAT(3, 4);

/* Case-insensitive string functions */
int strcasecmp_safe(const char *s1, const char *s2);
int strncasecmp_safe(const char *s1, const char *s2, size_t n);
char* strcasestr_safe(const char *haystack, const char *needle);

/* String conversion utilities */
bool string_to_int(const char *str, int *value);
bool string_to_long(const char *str, long *value);
bool string_to_double(const char *str, double *value);
bool string_to_bool(const char *str, bool *value);
bool string_to_size(const char *str, size_t *value);

/* File utilities */
bool file_exists(const char *filename);
bool directory_exists(const char *dirname);
bool is_regular_file(const char *filename);
bool is_directory(const char *dirname);
bool is_readable(const char *filename);
bool is_writable(const char *filename);
bool is_executable(const char *filename);
size_t get_file_size(const char *filename);
time_t get_file_mtime(const char *filename);

/* File operations */
result_code_t create_directory_recursive(const char *path, mode_t mode);
result_code_t copy_file(const char *src, const char *dest);
result_code_t move_file(const char *src, const char *dest);
result_code_t delete_file(const char *filename);
result_code_t delete_directory_recursive(const char *dirname);

/* File content utilities */
result_code_t read_file_to_string(const char *filename, char **content, size_t *size);
result_code_t write_string_to_file(const char *filename, const char *content);
result_code_t append_string_to_file(const char *filename, const char *content);

/* Path manipulation */
char* get_basename(const char *path, char *buffer, size_t buffer_size);
char* get_dirname(const char *path, char *buffer, size_t buffer_size);
char* get_file_extension(const char *filename, char *buffer, size_t buffer_size);
char* remove_file_extension(const char *filename, char *buffer, size_t buffer_size);
char* join_paths(const char *dir, const char *filename, char *buffer, size_t buffer_size);
char* resolve_path(const char *path, char *buffer, size_t buffer_size);

/* Network utilities */
bool is_valid_ipv4(const char *ip);
bool is_valid_port(int port);
const char* sockaddr_to_string(const struct sockaddr *addr, char *buffer, size_t buffer_size);
int string_to_sockaddr(const char *str, struct sockaddr *addr, socklen_t *addr_len);

/* Process utilities */
pid_t execute_command(const char *command, char *const argv[], char *const envp[]);
int wait_for_process(pid_t pid, int *exit_status, int timeout_seconds);
bool is_process_alive(pid_t pid);
int kill_process_tree(pid_t pid, int signal);
int get_process_status(pid_t pid, char *buffer, size_t buffer_size);

/* Memory utilities */
void* safe_malloc(size_t size);
void* safe_calloc(size_t nmemb, size_t size);
void* safe_realloc(void *ptr, size_t size);
char* safe_strdup(const char *str);
void secure_zero_memory(void *ptr, size_t size);

/* Buffer management */
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
    size_t position;
} buffer_t;

buffer_t* buffer_create(size_t initial_capacity);
void buffer_destroy(buffer_t *buffer);
result_code_t buffer_append(buffer_t *buffer, const void *data, size_t size);
result_code_t buffer_append_string(buffer_t *buffer, const char *str);
result_code_t buffer_append_printf(buffer_t *buffer, const char *format, ...) PRINTF_FORMAT(2, 3);
void buffer_clear(buffer_t *buffer);
void buffer_reset(buffer_t *buffer);

/* Random number utilities */
void init_random(void);
uint32_t random_uint32(void);
uint64_t random_uint64(void);
double random_double(void);
void random_bytes(void *buffer, size_t size);
char* generate_random_string(char *buffer, size_t length, const char *charset);

/* Checksum and hashing */
uint32_t crc32(const void *data, size_t size);
uint32_t hash_string(const char *str);
uint64_t hash_data(const void *data, size_t size);

/* Base64 encoding/decoding */
size_t base64_encode_length(size_t input_length);
size_t base64_decode_length(const char *input);
result_code_t base64_encode(const void *input, size_t input_length, char *output, size_t output_size);
result_code_t base64_decode(const char *input, void *output, size_t output_size, size_t *decoded_length);

/* URL encoding/decoding */
size_t url_encode_length(const char *input);
size_t url_decode_length(const char *input);
result_code_t url_encode(const char *input, char *output, size_t output_size);
result_code_t url_decode(const char *input, char *output, size_t output_size);

/* JSON utilities (simple parser) */
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

typedef struct json_value json_value_t;

json_value_t* json_parse(const char *input);
void json_free(json_value_t *value);
json_type_t json_get_type(const json_value_t *value);
bool json_get_bool(const json_value_t *value, bool default_value);
double json_get_number(const json_value_t *value, double default_value);
const char* json_get_string(const json_value_t *value, const char *default_value);
json_value_t* json_get_object_item(const json_value_t *object, const char *key);
json_value_t* json_get_array_item(const json_value_t *array, size_t index);
size_t json_get_array_size(const json_value_t *array);

/* Configuration management */
typedef struct config_item config_item_t;

config_item_t* config_load(const char *filename);
void config_free(config_item_t *config);
const char* config_get_string(const config_item_t *config, const char *key, const char *default_value);
int config_get_int(const config_item_t *config, const char *key, int default_value);
double config_get_double(const config_item_t *config, const char *key, double default_value);
bool config_get_bool(const config_item_t *config, const char *key, bool default_value);
result_code_t config_set_string(config_item_t *config, const char *key, const char *value);
result_code_t config_set_int(config_item_t *config, const char *key, int value);
result_code_t config_set_double(config_item_t *config, const char *key, double value);
result_code_t config_set_bool(config_item_t *config, const char *key, bool value);
result_code_t config_save(const config_item_t *config, const char *filename);

/* Error handling */
extern __thread char g_last_error_message[256];
extern __thread result_code_t g_last_error_code;

void set_error(result_code_t code, const char *format, ...) PRINTF_FORMAT(2, 3);
result_code_t get_last_error_code(void);
const char* get_last_error_message(void);
const char* error_code_to_string(result_code_t code);
void clear_error(void);

/* Debug and assertion macros */
#ifdef DEBUG_MODE
    #define ASSERT(condition) \
        do { \
            if (UNLIKELY(!(condition))) { \
                fprintf(stderr, "Assertion failed: %s at %s:%d in %s()\n", \
                       #condition, __FILE__, __LINE__, __func__); \
                abort(); \
            } \
        } while(0)
    
    #define DEBUG_PRINT(fmt, ...) \
        fprintf(stderr, "[DEBUG %s:%d] " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
    #define ASSERT(condition) ((void)0)
    #define DEBUG_PRINT(fmt, ...) ((void)0)
#endif

/* Performance monitoring */
typedef struct {
    uint64_t function_calls;
    uint64_t total_time_us;
    uint64_t min_time_us;
    uint64_t max_time_us;
    char name[64];
} perf_counter_t;

#ifdef ENABLE_PROFILING
    #define PERF_COUNTER_DECLARE(name) \
        static perf_counter_t __perf_##name = {0, 0, UINT64_MAX, 0, #name}
    
    #define PERF_COUNTER_START(name) \
        uint64_t __start_##name = get_current_time_us()
    
    #define PERF_COUNTER_END(name) \
        do { \
            uint64_t __elapsed = get_current_time_us() - __start_##name; \
            __perf_##name.function_calls++; \
            __perf_##name.total_time_us += __elapsed; \
            if (__elapsed < __perf_##name.min_time_us) \
                __perf_##name.min_time_us = __elapsed; \
            if (__elapsed > __perf_##name.max_time_us) \
                __perf_##name.max_time_us = __elapsed; \
        } while(0)
    
    #define PERF_COUNTER_REPORT(name) \
        printf("PERF %s: calls=%lu, total=%lu us, avg=%.2f us, min=%lu us, max=%lu us\n", \
               __perf_##name.name, __perf_##name.function_calls, \
               __perf_##name.total_time_us, \
               (double)__perf_##name.total_time_us / __perf_##name.function_calls, \
               __perf_##name.min_time_us, __perf_##name.max_time_us)
#else
    #define PERF_COUNTER_DECLARE(name)
    #define PERF_COUNTER_START(name)
    #define PERF_COUNTER_END(name)
    #define PERF_COUNTER_REPORT(name)
#endif

#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */