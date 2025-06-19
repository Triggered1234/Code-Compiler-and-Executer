/**
 * @file utils.h
 * @brief Utility functions for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* String utilities */
char* safe_strdup(const char *str);
char* safe_strndup(const char *str, size_t n);
char* trim_whitespace(char *str);
char* ltrim_whitespace(char *str);
char* rtrim_whitespace(char *str);
bool starts_with(const char *str, const char *prefix);
bool ends_with(const char *str, const char *suffix);
bool contains_substring(const char *str, const char *substr);
int safe_snprintf(char *buffer, size_t size, const char *format, ...);
int case_insensitive_compare(const char *s1, const char *s2);

/* String array utilities */
char** split_string(const char *str, const char *delimiter, int *count);
void free_string_array(char **array, int count);
char* join_strings(const char **strings, int count, const char *separator);
int find_string_in_array(const char **array, int count, const char *target);

/* Number parsing utilities */
bool parse_int(const char *str, int *value);
bool parse_long(const char *str, long *value);
bool parse_uint32(const char *str, uint32_t *value);
bool parse_uint64(const char *str, uint64_t *value);
bool parse_double(const char *str, double *value);
bool parse_bool(const char *str, bool *value);

/* Number formatting utilities */
char* format_number_with_commas(long long number, char *buffer, size_t size);
char* format_bytes(uint64_t bytes, char *buffer, size_t size);
char* format_duration(double seconds, char *buffer, size_t size);
char* format_percentage(double value, char *buffer, size_t size);

/* Time utilities */
uint64_t get_timestamp_ms(void);
uint64_t get_timestamp_us(void);
void get_current_timespec(struct timespec *ts);
double timespec_diff(const struct timespec *start, const struct timespec *end);
char* format_timestamp(time_t timestamp, char *buffer, size_t size);
char* format_iso_timestamp(time_t timestamp, char *buffer, size_t size);
time_t parse_iso_timestamp(const char *timestamp_str);

/* File utilities */
bool file_exists(const char *path);
bool is_regular_file(const char *path);
bool is_directory(const char *path);
bool is_readable(const char *path);
bool is_writable(const char *path);
bool is_executable(const char *path);
off_t get_file_size(const char *path);
time_t get_file_mtime(const char *path);
int create_directory_recursive(const char *path, mode_t mode);
int copy_file(const char *src, const char *dest);
int move_file(const char *src, const char *dest);
int delete_file(const char *path);

/* Path utilities */
char* get_basename(const char *path, char *buffer, size_t size);
char* get_dirname(const char *path, char *buffer, size_t size);
char* get_file_extension(const char *path, char *buffer, size_t size);
char* remove_extension(const char *path, char *buffer, size_t size);
char* join_path(const char *dir, const char *file, char *buffer, size_t size);
char* normalize_path(const char *path, char *buffer, size_t size);
char* resolve_path(const char *path, char *buffer, size_t size);

/* File content utilities */
int read_entire_file(const char *path, char **content, size_t *size);
int write_entire_file(const char *path, const void *content, size_t size);
int append_to_file(const char *path, const void *content, size_t size);
int read_file_lines(const char *path, char ***lines, int *line_count);
void free_file_lines(char **lines, int line_count);

/* Temporary file utilities */
char* create_temp_file(const char *prefix, const char *suffix, char *buffer, size_t size);
char* create_temp_directory(const char *prefix, char *buffer, size_t size);
int cleanup_temp_files(const char *pattern);

/* Memory utilities */
void* safe_malloc(size_t size);
void* safe_calloc(size_t nmemb, size_t size);
void* safe_realloc(void *ptr, size_t size);
void safe_free(void **ptr);
void secure_zero_memory(void *ptr, size_t size);

/* Buffer utilities */
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} buffer_t;

buffer_t* buffer_create(size_t initial_capacity);
void buffer_destroy(buffer_t *buffer);
int buffer_append(buffer_t *buffer, const void *data, size_t size);
int buffer_append_string(buffer_t *buffer, const char *str);
int buffer_append_printf(buffer_t *buffer, const char *format, ...);
void buffer_clear(buffer_t *buffer);
char* buffer_get_string(buffer_t *buffer);

/* Logging utilities */
typedef enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_ERROR = 4,
    LOG_LEVEL_FATAL = 5
} log_level_t;

typedef struct {
    FILE *file;
    int level;
    bool use_colors;
    bool show_timestamp;
    bool show_location;
    char *buffer;
    size_t buffer_size;
} logger_t;

logger_t* logger_create(const char *filename, log_level_t level);
void logger_destroy(logger_t *logger);
void logger_set_level(logger_t *logger, log_level_t level);
void logger_set_colors(logger_t *logger, bool enabled);
void logger_log(logger_t *logger, log_level_t level, const char *file, 
               int line, const char *func, const char *format, ...);

/* Process utilities */
pid_t spawn_process(const char *program, char *const argv[], char *const envp[]);
int wait_for_process(pid_t pid, int *exit_status, int timeout_seconds);
bool is_process_running(pid_t pid);
int kill_process(pid_t pid, int signal);
int get_process_exit_status(pid_t pid);

/* System information utilities */
long get_system_memory_total(void);
long get_system_memory_available(void);
int get_cpu_count(void);
double get_cpu_usage(void);
double get_load_average(void);
char* get_hostname(char *buffer, size_t size);
char* get_username(char *buffer, size_t size);

/* Network utilities */
bool is_valid_ipv4(const char *ip);
bool is_valid_ipv6(const char *ip);
bool is_valid_port(int port);
int resolve_hostname(const char *hostname, char *ip_buffer, size_t size);
int get_local_ip(char *ip_buffer, size_t size);

/* Checksum and hashing utilities */
uint32_t crc32(const void *data, size_t size);
uint32_t hash_string(const char *str);
uint64_t hash_data(const void *data, size_t size);
void md5_hash(const void *data, size_t size, unsigned char *hash);
void sha256_hash(const void *data, size_t size, unsigned char *hash);

/* Base64 encoding/decoding */
size_t base64_encode_size(size_t input_size);
size_t base64_decode_size(const char *input);
int base64_encode(const void *input, size_t input_size, char *output, size_t output_size);
int base64_decode(const char *input, void *output, size_t output_size);

/* URL encoding/decoding */
size_t url_encode_size(const char *input);
int url_encode(const char *input, char *output, size_t output_size);
int url_decode(const char *input, char *output, size_t output_size);

/* Random utilities */
void random_init(void);
uint32_t random_uint32(void);
uint64_t random_uint64(void);
double random_double(void);
void random_bytes(void *buffer, size_t size);
char* random_string(char *buffer, size_t length, const char *charset);

/* UUID utilities */
typedef struct {
    uint8_t bytes[16];
} uuid_t;

void uuid_generate(uuid_t *uuid);
char* uuid_to_string(const uuid_t *uuid, char *buffer, size_t size);
int uuid_from_string(const char *str, uuid_t *uuid);
bool uuid_is_null(const uuid_t *uuid);
int uuid_compare(const uuid_t *a, const uuid_t *b);

/* Configuration file utilities */
typedef struct config_entry {
    char *key;
    char *value;
    struct config_entry *next;
} config_entry_t;

typedef struct {
    config_entry_t *entries;
    char *filename;
    bool modified;
} config_t;

config_t* config_load(const char *filename);
void config_destroy(config_t *config);
const char* config_get_string(config_t *config, const char *key, const char *default_value);
int config_get_int(config_t *config, const char *key, int default_value);
bool config_get_bool(config_t *config, const char *key, bool default_value);
double config_get_double(config_t *config, const char *key, double default_value);
void config_set_string(config_t *config, const char *key, const char *value);
void config_set_int(config_t *config, const char *key, int value);
void config_set_bool(config_t *config, const char *key, bool value);
void config_set_double(config_t *config, const char *key, double value);
int config_save(config_t *config);

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

json_value_t* json_parse(const char *text);
void json_free(json_value_t *value);
json_type_t json_get_type(const json_value_t *value);
bool json_get_bool(const json_value_t *value);
double json_get_number(const json_value_t *value);
const char* json_get_string(const json_value_t *value);
json_value_t* json_get_object_member(const json_value_t *object, const char *key);
json_value_t* json_get_array_element(const json_value_t *array, size_t index);
size_t json_get_array_size(const json_value_t *array);
size_t json_get_object_size(const json_value_t *object);

/* Command line argument parsing */
typedef struct {
    const char *short_name;
    const char *long_name;
    bool has_argument;
    const char *description;
    void *target;
    enum { ARG_INT, ARG_STRING, ARG_BOOL, ARG_DOUBLE } type;
} arg_option_t;

typedef struct {
    arg_option_t *options;
    int option_count;
    char **remaining_args;
    int remaining_count;
} args_parser_t;

args_parser_t* args_parser_create(void);
void args_parser_destroy(args_parser_t *parser);
void args_parser_add_option(args_parser_t *parser, const char *short_name,
                           const char *long_name, bool has_argument,
                           const char *description, void *target, int type);
int args_parser_parse(args_parser_t *parser, int argc, char *argv[]);
void args_parser_print_help(args_parser_t *parser, const char *program_name);

/* Performance measurement utilities */
typedef struct {
    struct timespec start_time;
    struct timespec end_time;
    bool running;
} stopwatch_t;

void stopwatch_start(stopwatch_t *sw);
void stopwatch_stop(stopwatch_t *sw);
double stopwatch_elapsed_ms(const stopwatch_t *sw);
double stopwatch_elapsed_us(const stopwatch_t *sw);
void stopwatch_reset(stopwatch_t *sw);

/* Thread-safe queue implementation */
typedef struct queue_node {
    void *data;
    struct queue_node *next;
} queue_node_t;

typedef struct {
    queue_node_t *head;
    queue_node_t *tail;
    size_t size;
    size_t max_size;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} thread_safe_queue_t;

thread_safe_queue_t* queue_create(size_t max_size);
void queue_destroy(thread_safe_queue_t *queue, void (*free_func)(void*));
int queue_enqueue(thread_safe_queue_t *queue, void *data);
void* queue_dequeue(thread_safe_queue_t *queue);
void* queue_dequeue_timeout(thread_safe_queue_t *queue, int timeout_ms);
size_t queue_size(thread_safe_queue_t *queue);
bool queue_is_empty(thread_safe_queue_t *queue);
bool queue_is_full(thread_safe_queue_t *queue);

/* Reference counting utilities */
typedef struct {
    void *data;
    int ref_count;
    void (*destructor)(void*);
    pthread_mutex_t mutex;
} ref_counted_t;

ref_counted_t* ref_create(void *data, void (*destructor)(void*));
ref_counted_t* ref_retain(ref_counted_t *ref);
void ref_release(ref_counted_t *ref);
void* ref_get_data(ref_counted_t *ref);

/* Error handling utilities */
typedef struct {
    int code;
    char message[256];
    char file[64];
    int line;
    char function[64];
} error_info_t;

extern __thread error_info_t g_last_error;

void set_error(int code, const char *message, const char *file, int line, const char *func);
int get_last_error_code(void);
const char* get_last_error_message(void);
void clear_error(void);

#define SET_ERROR(code, message) set_error(code, message, __FILE__, __LINE__, __func__)
#define CLEAR_ERROR() clear_error()

/* Validation utilities */
bool is_valid_email(const char *email);
bool is_valid_filename_char(char c);
bool is_valid_identifier(const char *name);
bool is_alphanumeric(const char *str);
bool is_numeric(const char *str);
bool is_hexadecimal(const char *str);

/* Platform abstraction */
#ifdef _WIN32
    #define PATH_SEPARATOR '\\'
    #define PATH_SEPARATOR_STR "\\"
    #define LINE_ENDING "\r\n"
    #define EXECUTABLE_EXTENSION ".exe"
#else
    #define PATH_SEPARATOR '/'
    #define PATH_SEPARATOR_STR "/"
    #define LINE_ENDING "\n"
    #define EXECUTABLE_EXTENSION ""
#endif

/* Compiler attribute macros */
#ifdef __GNUC__
    #define PRINTF_FORMAT(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
    #define UNUSED_PARAM __attribute__((unused))
    #define LIKELY(x) __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define PRINTF_FORMAT(fmt_idx, arg_idx)
    #define UNUSED_PARAM
    #define LIKELY(x) (x)
    #define UNLIKELY(x) (x)
#endif

/* Utility macros */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(val, min_val, max_val) MAX(min_val, MIN(val, max_val))
#define SWAP(a, b) do { typeof(a) _temp = (a); (a) = (b); (b) = _temp; } while(0)
#define ROUNDUP(x, align) (((x) + (align) - 1) & ~((align) - 1))

/* Memory safety macros */
#define SAFE_FREE(ptr) do { if (ptr) { free(ptr); (ptr) = NULL; } } while(0)
#define SAFE_CLOSE(fd) do { if ((fd) >= 0) { close(fd); (fd) = -1; } } while(0)
#define SAFE_FCLOSE(fp) do { if (fp) { fclose(fp); (fp) = NULL; } } while(0)

/* String safety macros */
#define SAFE_STRNCPY(dest, src, size) do { \
    strncpy((dest), (src), (size) - 1); \
    (dest)[(size) - 1] = '\0'; \
} while(0)

#define SAFE_STRNCAT(dest, src, size) do { \
    size_t _len = strlen(dest); \
    if (_len < (size) - 1) { \
        strncat((dest), (src), (size) - _len - 1); \
    } \
} while(0)

/* Debug utilities */
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
        fprintf(stderr, "[DEBUG %s:%d] " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
    #define ASSERT(condition) \
        do { \
            if (!(condition)) { \
                fprintf(stderr, "Assertion failed: %s at %s:%d\n", \
                       #condition, __FILE__, __LINE__); \
                abort(); \
            } \
        } while(0)
#else
    #define DEBUG_PRINT(fmt, ...) ((void)0)
    #define ASSERT(condition) ((void)0)
#endif

/* Benchmark utilities */
#define BENCHMARK_START(name) \
    struct timespec _bench_start_##name; \
    clock_gettime(CLOCK_MONOTONIC, &_bench_start_##name)

#define BENCHMARK_END(name) \
    do { \
        struct timespec _bench_end_##name; \
        clock_gettime(CLOCK_MONOTONIC, &_bench_end_##name); \
        double _elapsed = timespec_diff(&_bench_start_##name, &_bench_end_##name); \
        printf("BENCHMARK %s: %.3f ms\n", #name, _elapsed * 1000.0); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */