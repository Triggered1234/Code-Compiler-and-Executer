/**
 * @file server.h
 * @brief Server definitions and structures for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#ifndef SERVER_H
#define SERVER_H

#include "protocol.h"
#include <pthread.h>
#include <time.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Server configuration constants */
#define SERVER_VERSION "1.0.0"
#define MAX_CLIENTS 1000
#define MAX_ADMIN_CONNECTIONS 1
#define SOCKET_BACKLOG 128
#define DEFAULT_SERVER_PORT 8080
#define DEFAULT_ADMIN_SOCKET "/tmp/code_server_admin.sock"

/* Timeout values in seconds */
#define CLIENT_TIMEOUT 300      /* 5 minutes */
#define ADMIN_TIMEOUT 1800      /* 30 minutes */
#define COMPILE_TIMEOUT 60      /* 1 minute */
#define EXECUTION_TIMEOUT 30    /* 30 seconds */

/* Buffer sizes */
#define MAX_PATH_LEN 4096
#define MAX_COMMAND_LEN 1024
#define MAX_LOG_MESSAGE 2048

/* Job priorities */
#define JOB_PRIORITY_LOW 1
#define JOB_PRIORITY_NORMAL 5
#define JOB_PRIORITY_HIGH 10

/**
 * @brief Client connection state
 */
typedef enum {
    CLIENT_STATE_CONNECTING,
    CLIENT_STATE_AUTHENTICATED,
    CLIENT_STATE_IDLE,
    CLIENT_STATE_UPLOADING,
    CLIENT_STATE_PROCESSING,
    CLIENT_STATE_DISCONNECTING
} client_state_t;

/**
 * @brief Job states
 */
typedef enum {
    JOB_STATE_QUEUED,
    JOB_STATE_RUNNING,
    JOB_STATE_COMPLETED,
    JOB_STATE_FAILED,
    JOB_STATE_CANCELLED,
    JOB_STATE_TIMEOUT
} job_state_t;

/**
 * @brief Client information structure
 */
typedef struct client_info {
    int socket_fd;
    uint32_t client_id;
    client_state_t state;
    time_t connect_time;
    time_t last_activity;
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
    char client_name[64];
    char client_platform[32];
    uint32_t active_jobs;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    pthread_mutex_t client_mutex;
    struct client_info *next;
} client_info_t;

/**
 * @brief Job information structure
 */
typedef struct job_info {
    uint32_t job_id;
    uint32_t client_id;
    uint32_t correlation_id;
    job_state_t state;
    language_t language;
    execution_mode_t mode;
    time_t submit_time;
    time_t start_time;
    time_t end_time;
    pid_t process_id;
    int exit_code;
    char source_file[MAX_FILENAME_SIZE];
    char output_file[MAX_FILENAME_SIZE];
    char error_file[MAX_FILENAME_SIZE];
    char compiler_args[MAX_COMMAND_SIZE];
    char execution_args[MAX_COMMAND_SIZE];
    size_t output_size;
    size_t error_size;
    int priority;
} job_info_t;

/**
 * @brief Job queue node
 */
typedef struct job_node {
    job_info_t job;
    struct job_node *next;
} job_node_t;

/**
 * @brief Job queue structure
 */
typedef struct {
    job_node_t *head;
    job_node_t *tail;
    size_t count;
    size_t max_size;
} job_queue_t;

/**
 * @brief Server statistics
 */
typedef struct {
    time_t start_time;
    uint32_t total_clients;
    uint32_t active_clients;
    uint32_t max_concurrent_clients;
    uint32_t total_jobs;
    uint32_t active_jobs;
    uint32_t completed_jobs;
    uint32_t failed_jobs;
    uint32_t cancelled_jobs;
    uint64_t total_bytes_received;
    uint64_t total_bytes_sent;
    double avg_job_time;
    double avg_queue_time;
    uint32_t memory_usage_kb;
    float cpu_usage_percent;
} server_stats_t;

/**
 * @brief Server configuration
 */
typedef struct {
    int port;
    char admin_socket_path[MAX_PATH_LEN];
    char processing_dir[MAX_PATH_LEN];
    char outgoing_dir[MAX_PATH_LEN];
    char log_dir[MAX_PATH_LEN];
    char config_file[MAX_PATH_LEN];
    int max_clients;
    int client_timeout;
    int admin_timeout;
    int compile_timeout;
    int execution_timeout;
    int debug_mode;
    int daemon_mode;
    int verbose;
    size_t max_file_size;
    size_t max_output_size;
    int max_concurrent_jobs;
    int enable_sandbox;
} server_config_t;

/**
 * @brief Main server context
 */
typedef struct {
    server_config_t config;
    server_stats_t stats;
    
    /* Sockets */
    int inet_socket;
    int unix_socket;
    
    /* Thread handles */
    pthread_t admin_thread;
    pthread_t client_thread;
    pthread_t processor_thread;
    
    /* Client management */
    client_info_t *clients_head;
    pthread_mutex_t clients_mutex;
    uint32_t next_client_id;
    
    /* Job management */
    job_queue_t job_queue;
    pthread_mutex_t jobs_mutex;
    pthread_cond_t job_queue_cond;
    uint32_t next_job_id;
    
    /* Statistics */
    pthread_mutex_t stats_mutex;
    
    /* Control flags */
    volatile int shutdown_requested;
    volatile int reload_config;
} server_context_t;

/* External server context */
extern server_context_t g_server;
extern volatile sig_atomic_t g_shutdown_requested;

/* Thread handlers */
void* admin_thread_handler(void *arg);
void* client_thread_handler(void *arg);
void* job_processor_thread(void *arg);

/* Client management functions */
client_info_t* add_client(int socket_fd, const char *ip, int port);
void remove_client(uint32_t client_id);
client_info_t* find_client(uint32_t client_id);
void update_client_activity(uint32_t client_id);
void cleanup_inactive_clients(void);

/* Job management functions */
uint32_t add_job(const job_info_t *job);
job_info_t* get_next_job(void);
void update_job_state(uint32_t job_id, job_state_t state);
void complete_job(uint32_t job_id, int exit_code, const char *output_file, const char *error_file);
void cancel_job(uint32_t job_id);
job_info_t* find_job(uint32_t job_id);

/* Statistics functions */
void update_stats_client_connected(void);
void update_stats_client_disconnected(void);
void update_stats_job_submitted(void);
void update_stats_job_completed(double job_time);
void update_stats_job_failed(void);
void update_stats_bytes_transferred(uint64_t sent, uint64_t received);
server_stats_t get_server_stats(void);

/* Utility functions */
uint32_t generate_job_id(void);
uint32_t generate_client_id(void);
const char* job_state_to_string(job_state_t state);
const char* client_state_to_string(client_state_t state);

/* Logging functions */
int init_logging(void);
void cleanup_logging(void);
void log_message(int level, const char *file, int line, const char *func, const char *format, ...);

/* Logging macros */
#define LOG_ERROR(fmt, ...) log_message(0, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) log_message(1, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) log_message(2, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_message(3, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/* Configuration functions */
int load_config_file(const char *filename);
int save_config_file(const char *filename);
void set_default_config(void);
int validate_config(void);

/* Directory management */
int ensure_directory_exists(const char *path);
int cleanup_old_files(const char *dir, int max_age_hours);
char* get_unique_filename(const char *base_dir, const char *prefix, const char *extension);

/* Process management */
int execute_compiler(const job_info_t *job, char *output_buffer, size_t output_size, 
                    char *error_buffer, size_t error_size);
int kill_job_process(pid_t pid);
int is_process_running(pid_t pid);

/* Security functions */
int setup_sandbox(void);
int validate_source_code(const char *filename, language_t language);
int check_file_safety(const char *filename);

/* Network utility functions */
int set_socket_nonblocking(int sockfd);
int set_socket_timeout(int sockfd, int timeout_sec);
ssize_t safe_read(int fd, void *buf, size_t count);
ssize_t safe_write(int fd, const void *buf, size_t count);
int send_message(int sockfd, const message_t *msg);
int receive_message(int sockfd, message_t *msg);

/* File management functions */
int save_uploaded_file(const char *filename, const void *data, size_t size);
int load_result_file(const char *filename, void **data, size_t *size);
int delete_job_files(uint32_t job_id);
char* get_job_directory(uint32_t job_id);

/* Admin interface functions */
int handle_admin_command(int sockfd, const char *command, char *response, size_t response_size);
int get_client_list(char *buffer, size_t buffer_size);
int get_job_list(char *buffer, size_t buffer_size);
int get_server_status(char *buffer, size_t buffer_size);
int disconnect_client_by_id(uint32_t client_id);
int cancel_job_by_id(uint32_t job_id);

/* Compiler detection and management */
typedef struct {
    language_t language;
    char compiler_path[MAX_PATH_LEN];
    char default_args[MAX_COMMAND_LEN];
    char file_extension[16];
    int available;
} compiler_info_t;

extern compiler_info_t g_compilers[];
extern size_t g_compiler_count;

int detect_compilers(void);
compiler_info_t* get_compiler_info(language_t language);
int is_language_supported(language_t language);

/* Performance monitoring */
typedef struct {
    double cpu_usage;
    size_t memory_usage;
    size_t disk_usage;
    int active_processes;
    time_t last_update;
} system_resources_t;

int get_system_resources(system_resources_t *resources);
int monitor_job_resources(pid_t pid, system_resources_t *resources);

/* Error handling */
typedef enum {
    ERROR_NONE = 0,
    ERROR_INVALID_ARGUMENT,
    ERROR_MEMORY_ALLOCATION,
    ERROR_FILE_IO,
    ERROR_NETWORK,
    ERROR_PERMISSION,
    ERROR_TIMEOUT,
    ERROR_COMPILATION,
    ERROR_EXECUTION,
    ERROR_QUOTA_EXCEEDED,
    ERROR_UNSUPPORTED_LANGUAGE,
    ERROR_SECURITY_VIOLATION,
    ERROR_INTERNAL
} error_code_t;

const char* error_code_to_string(error_code_t code);
void set_last_error(error_code_t code, const char *message);
error_code_t get_last_error(char *message, size_t message_size);

/* Signal handling */
int setup_signal_handlers(void);
void signal_handler(int sig);
void handle_sigchld(int sig);

/* Daemon functions */
int daemonize(void);
int create_pid_file(const char *pid_file);
int remove_pid_file(const char *pid_file);

/* Validation functions */
int validate_filename(const char *filename);
int validate_language(const char *language_str, language_t *language);
int validate_client_request(const message_t *msg);
int validate_file_content(const char *filename, language_t language);

/* Quota management */
typedef struct {
    uint32_t client_id;
    size_t files_uploaded;
    size_t bytes_uploaded;
    uint32_t jobs_submitted;
    time_t period_start;
    time_t last_activity;
} client_quota_t;

int init_quota_system(void);
int check_client_quota(uint32_t client_id, size_t file_size);
void update_client_quota(uint32_t client_id, size_t bytes_used);
void cleanup_quota_records(void);

/* Event system for notifications */
typedef enum {
    EVENT_CLIENT_CONNECTED,
    EVENT_CLIENT_DISCONNECTED,
    EVENT_JOB_SUBMITTED,
    EVENT_JOB_STARTED,
    EVENT_JOB_COMPLETED,
    EVENT_JOB_FAILED,
    EVENT_SERVER_SHUTDOWN,
    EVENT_ADMIN_LOGIN,
    EVENT_ERROR_OCCURRED
} event_type_t;

typedef struct {
    event_type_t type;
    uint32_t client_id;
    uint32_t job_id;
    time_t timestamp;
    char message[256];
    void *data;
    size_t data_size;
} server_event_t;

int init_event_system(void);
void emit_event(event_type_t type, uint32_t client_id, uint32_t job_id, const char *message);
void cleanup_event_system(void);

/* Thread-safe memory management */
void* safe_malloc(size_t size);
void* safe_calloc(size_t nmemb, size_t size);
void* safe_realloc(void *ptr, size_t size);
void safe_free(void *ptr);
char* safe_strdup(const char *s);

/* Time utilities */
double time_diff(const struct timespec *start, const struct timespec *end);
void get_current_time(struct timespec *ts);
char* format_timestamp(time_t timestamp, char *buffer, size_t buffer_size);
char* format_duration(double seconds, char *buffer, size_t buffer_size);

/* String utilities */
char* trim_whitespace(char *str);
int split_string(const char *str, char delimiter, char **tokens, int max_tokens);
int escape_string(const char *input, char *output, size_t output_size);
int is_valid_identifier(const char *str);

/* Platform-specific functions */
#ifdef __linux__
int setup_inotify_watch(const char *path);
int get_process_memory_usage(pid_t pid);
int set_process_limits(pid_t pid);
#endif

#ifdef __APPLE__
int setup_kqueue_watch(const char *path);
#endif

/* Debugging and profiling */
#ifdef DEBUG_MODE
#define PROFILE_START(name) \
    struct timespec __prof_start_##name; \
    get_current_time(&__prof_start_##name)

#define PROFILE_END(name) \
    do { \
        struct timespec __prof_end_##name; \
        get_current_time(&__prof_end_##name); \
        double __prof_time = time_diff(&__prof_start_##name, &__prof_end_##name); \
        LOG_DEBUG("PROFILE: %s took %.3f ms", #name, __prof_time * 1000.0); \
    } while(0)

#define DEBUG_PRINT_STATS() \
    do { \
        server_stats_t stats = get_server_stats(); \
        LOG_DEBUG("Stats: clients=%d, jobs=%d, completed=%d, failed=%d", \
                 stats.active_clients, stats.active_jobs, \
                 stats.completed_jobs, stats.failed_jobs); \
    } while(0)
#else
#define PROFILE_START(name)
#define PROFILE_END(name)
#define DEBUG_PRINT_STATS()
#endif

/* Configuration macros */
#define CONFIG_GET_INT(key, default_val) \
    get_config_int(key, default_val)

#define CONFIG_GET_STRING(key, default_val, buffer, size) \
    get_config_string(key, default_val, buffer, size)

#define CONFIG_GET_BOOL(key, default_val) \
    get_config_bool(key, default_val)

/* Helper macros */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(val, min_val, max_val) MAX(min_val, MIN(val, max_val))

/* Memory barrier for thread safety */
#define MEMORY_BARRIER() __sync_synchronize()

/* Atomic operations */
#define ATOMIC_INCREMENT(ptr) __sync_add_and_fetch(ptr, 1)
#define ATOMIC_DECREMENT(ptr) __sync_sub_and_fetch(ptr, 1)
#define ATOMIC_LOAD(ptr) __sync_fetch_and_add(ptr, 0)
#define ATOMIC_STORE(ptr, val) __sync_lock_test_and_set(ptr, val)

#ifdef __cplusplus
}
#endif

#endif /* SERVER_H */