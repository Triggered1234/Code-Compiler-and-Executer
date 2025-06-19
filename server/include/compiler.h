/**
 * @file compiler.h
 * @brief Compiler service definitions for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#ifndef COMPILER_H
#define COMPILER_H

#include "protocol.h"
#include "common.h"
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compiler configuration constants */
#define MAX_COMPILER_PATH_LEN 512
#define MAX_COMPILER_ARGS_LEN 1024
#define MAX_COMPILER_OUTPUT 8192
#define MAX_COMPILATION_TIME 300  /* 5 minutes max */
#define MAX_EXECUTION_TIME 60     /* 1 minute max */

/* Supported programming languages */
typedef enum {
    COMPILER_UNKNOWN = 0,
    COMPILER_C,
    COMPILER_CPP,
    COMPILER_JAVA,
    COMPILER_PYTHON,
    COMPILER_JAVASCRIPT,
    COMPILER_GO,
    COMPILER_RUST,
    COMPILER_MAX
} compiler_type_t;

/* Compilation status */
typedef enum {
    COMPILE_STATUS_PENDING = 0,
    COMPILE_STATUS_COMPILING,
    COMPILE_STATUS_COMPILED,
    COMPILE_STATUS_RUNNING,
    COMPILE_STATUS_COMPLETED,
    COMPILE_STATUS_FAILED,
    COMPILE_STATUS_TIMEOUT,
    COMPILE_STATUS_CANCELLED
} compile_status_t;

/* Execution modes */
typedef enum {
    EXEC_MODE_COMPILE_ONLY = 0,
    EXEC_MODE_COMPILE_AND_RUN,
    EXEC_MODE_INTERPRET_ONLY,
    EXEC_MODE_SYNTAX_CHECK
} execution_mode_t;

/**
 * @brief Compiler information structure
 */
typedef struct {
    compiler_type_t type;
    char name[32];
    char executable_path[MAX_COMPILER_PATH_LEN];
    char version[64];
    char default_args[MAX_COMPILER_ARGS_LEN];
    char file_extensions[128];  /* Comma-separated list */
    bool available;
    bool supports_debugging;
    bool supports_optimization;
    int detection_priority;
} compiler_info_t;

/**
 * @brief Compilation job structure
 */
typedef struct {
    uint32_t job_id;
    uint32_t client_id;
    compiler_type_t compiler_type;
    execution_mode_t exec_mode;
    compile_status_t status;
    
    /* File paths */
    char source_file[MAX_FILENAME_SIZE];
    char executable_file[MAX_FILENAME_SIZE];
    char output_file[MAX_FILENAME_SIZE];
    char error_file[MAX_FILENAME_SIZE];
    
    /* Compilation settings */
    char compiler_args[MAX_COMPILER_ARGS_LEN];
    char execution_args[MAX_COMPILER_ARGS_LEN];
    bool enable_debugging;
    bool enable_optimization;
    int optimization_level;
    
    /* Timing information */
    time_t submit_time;
    time_t compile_start_time;
    time_t compile_end_time;
    time_t exec_start_time;
    time_t exec_end_time;
    
    /* Process information */
    pid_t compile_pid;
    pid_t exec_pid;
    int compile_exit_code;
    int exec_exit_code;
    
    /* Resource usage */
    size_t memory_used;
    double cpu_time_used;
    size_t output_size;
    size_t error_size;
    
    /* Security sandbox */
    bool use_sandbox;
    char sandbox_dir[MAX_PATH_LEN];
    uid_t sandbox_uid;
    gid_t sandbox_gid;
} compilation_job_t;

/**
 * @brief Compiler statistics
 */
typedef struct {
    uint32_t total_jobs;
    uint32_t successful_compilations;
    uint32_t failed_compilations;
    uint32_t successful_executions;
    uint32_t failed_executions;
    uint32_t timeouts;
    double avg_compile_time;
    double avg_execution_time;
    size_t total_memory_used;
    time_t last_reset_time;
} compiler_stats_t;

/* Global compiler registry */
extern compiler_info_t g_compilers[COMPILER_MAX];
extern size_t g_compiler_count;
extern compiler_stats_t g_compiler_stats;

/* Compiler detection and initialization */
int compiler_service_init(void);
void compiler_service_cleanup(void);
int detect_available_compilers(void);
int register_compiler(const compiler_info_t* compiler);
compiler_info_t* get_compiler_by_type(compiler_type_t type);
compiler_info_t* get_compiler_by_extension(const char* extension);

/* Language detection */
compiler_type_t detect_language_from_filename(const char* filename);
compiler_type_t detect_language_from_content(const char* content, size_t content_size);
const char* compiler_type_to_string(compiler_type_t type);
compiler_type_t string_to_compiler_type(const char* type_str);

/* Compilation job management */
compilation_job_t* create_compilation_job(uint32_t job_id, uint32_t client_id);
void destroy_compilation_job(compilation_job_t* job);
int prepare_compilation_environment(compilation_job_t* job);
int cleanup_compilation_environment(compilation_job_t* job);

/* Compilation process */
int compile_source_code(compilation_job_t* job);
int execute_compiled_program(compilation_job_t* job);
int syntax_check_only(compilation_job_t* job);
int interpret_source_code(compilation_job_t* job);

/* Process control */
int start_compilation_process(compilation_job_t* job);
int start_execution_process(compilation_job_t* job);
int wait_for_process_completion(pid_t pid, int* exit_code, int timeout_seconds);
int kill_compilation_job(compilation_job_t* job, bool force);
bool is_job_running(const compilation_job_t* job);

/* Security and sandboxing */
int setup_compilation_sandbox(compilation_job_t* job);
int apply_resource_limits(pid_t pid, size_t memory_limit, int cpu_limit);
int validate_source_code_safety(const char* source_file, compiler_type_t type);
int check_executable_safety(const char* executable_file);

/* File management */
int prepare_source_file(compilation_job_t* job, const void* source_data, size_t data_size);
int read_compilation_output(compilation_job_t* job, char** output, size_t* output_size);
int read_compilation_errors(compilation_job_t* job, char** errors, size_t* error_size);
int read_execution_output(compilation_job_t* job, char** output, size_t* output_size);

/* Compiler configuration */
int load_compiler_config(const char* config_file);
int save_compiler_config(const char* config_file);
int set_compiler_option(compiler_type_t type, const char* option, const char* value);
const char* get_compiler_option(compiler_type_t type, const char* option);

/* Statistics and monitoring */
void update_compiler_stats(const compilation_job_t* job);
void reset_compiler_stats(void);
compiler_stats_t get_compiler_stats(void);
void print_compiler_stats(void);

/* Utility functions */
const char* compile_status_to_string(compile_status_t status);
const char* execution_mode_to_string(execution_mode_t mode);
bool is_source_file_valid(const char* filename);
bool is_executable_file(const char* filename);
char* generate_unique_executable_name(const char* source_file);
char* get_file_extension(const char* filename);

/* Language-specific compilation functions */
int compile_c_source(compilation_job_t* job);
int compile_cpp_source(compilation_job_t* job);
int compile_java_source(compilation_job_t* job);
int compile_go_source(compilation_job_t* job);
int compile_rust_source(compilation_job_t* job);

/* Language-specific execution functions */
int execute_c_program(compilation_job_t* job);
int execute_cpp_program(compilation_job_t* job);
int execute_java_program(compilation_job_t* job);
int execute_python_script(compilation_job_t* job);
int execute_javascript_script(compilation_job_t* job);
int execute_go_program(compilation_job_t* job);
int execute_rust_program(compilation_job_t* job);

/* Compiler detection functions */
bool detect_gcc_compiler(compiler_info_t* info);
bool detect_gpp_compiler(compiler_info_t* info);
bool detect_javac_compiler(compiler_info_t* info);
bool detect_python_interpreter(compiler_info_t* info);
bool detect_node_interpreter(compiler_info_t* info);
bool detect_go_compiler(compiler_info_t* info);
bool detect_rust_compiler(compiler_info_t* info);

/* Error handling */
typedef enum {
    COMPILER_ERROR_NONE = 0,
    COMPILER_ERROR_NOT_FOUND,
    COMPILER_ERROR_INVALID_SOURCE,
    COMPILER_ERROR_COMPILATION_FAILED,
    COMPILER_ERROR_EXECUTION_FAILED,
    COMPILER_ERROR_TIMEOUT,
    COMPILER_ERROR_MEMORY_LIMIT,
    COMPILER_ERROR_SECURITY_VIOLATION,
    COMPILER_ERROR_IO_ERROR,
    COMPILER_ERROR_INTERNAL
} compiler_error_t;

const char* compiler_error_to_string(compiler_error_t error);
void set_compiler_error(compiler_error_t error, const char* details);
compiler_error_t get_last_compiler_error(char* details, size_t details_size);

/* Debugging and profiling */
#ifdef DEBUG_MODE
void dump_compilation_job(const compilation_job_t* job);
void dump_compiler_info(const compiler_info_t* info);
void trace_compilation_process(const compilation_job_t* job, const char* stage);
#else
#define dump_compilation_job(job) ((void)0)
#define dump_compiler_info(info) ((void)0)
#define trace_compilation_process(job, stage) ((void)0)
#endif

/* Macros for compiler detection */
#define COMPILER_DETECT_DECLARE(name) \
    bool detect_##name##_compiler(compiler_info_t* info)

#define COMPILER_DETECT_IMPLEMENT(name, executable, version_arg) \
    bool detect_##name##_compiler(compiler_info_t* info) { \
        char command[512]; \
        snprintf(command, sizeof(command), "which %s >/dev/null 2>&1", executable); \
        if (system(command) != 0) return false; \
        \
        snprintf(info->executable_path, sizeof(info->executable_path), "%s", executable); \
        snprintf(command, sizeof(command), "%s %s 2>&1", executable, version_arg); \
        /* Get version info */ \
        FILE* fp = popen(command, "r"); \
        if (fp) { \
            fgets(info->version, sizeof(info->version), fp); \
            pclose(fp); \
        } \
        info->available = true; \
        return true; \
    }

/* Resource monitoring */
typedef struct {
    size_t peak_memory_usage;
    double cpu_time_used;
    size_t files_created;
    size_t bytes_written;
    int processes_spawned;
} resource_usage_t;

int monitor_job_resources(compilation_job_t* job, resource_usage_t* usage);
int enforce_resource_limits(compilation_job_t* job);
bool check_resource_quota(uint32_t client_id);

#ifdef __cplusplus
}
#endif

#endif /* COMPILER_H */