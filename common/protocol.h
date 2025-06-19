/**
 * @file protocol.h
 * @brief Protocol definitions for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Protocol version */
#define PROTOCOL_VERSION_MAJOR 1
#define PROTOCOL_VERSION_MINOR 0
#define PROTOCOL_VERSION_PATCH 0

/* Protocol constants */
#define PROTOCOL_MAGIC 0x43434545  /* "CCEE" */
#define MAX_MESSAGE_SIZE (16 * 1024 * 1024)  /* 16MB */
#define MAX_FILENAME_SIZE 256
#define MAX_COMMAND_SIZE 1024
#define MAX_ERROR_MESSAGE_SIZE 4096

/* Message types */
typedef enum {
    MSG_INVALID = 0,
    
    /* Client messages */
    MSG_HELLO = 1,
    MSG_FILE_UPLOAD_START = 2,
    MSG_FILE_UPLOAD_CHUNK = 3,
    MSG_FILE_UPLOAD_END = 4,
    MSG_COMPILE_REQUEST = 5,
    MSG_STATUS_REQUEST = 6,
    MSG_RESULT_REQUEST = 7,
    MSG_PING = 8,
    
    /* Server responses */
    MSG_ACK = 100,
    MSG_NACK = 101,
    MSG_ERROR = 102,
    MSG_COMPILE_RESPONSE = 103,
    MSG_STATUS_RESPONSE = 104,
    MSG_RESULT_RESPONSE = 105,
    MSG_PONG = 106,
    
    /* Admin messages */
    MSG_ADMIN_CONNECT = 200,
    MSG_ADMIN_DISCONNECT = 201,
    MSG_ADMIN_LIST_CLIENTS = 202,
    MSG_ADMIN_LIST_JOBS = 203,
    MSG_ADMIN_SERVER_STATS = 204,
    MSG_ADMIN_DISCONNECT_CLIENT = 205,
    MSG_ADMIN_KILL_JOB = 206,
    MSG_ADMIN_SERVER_SHUTDOWN = 207,
    MSG_ADMIN_CONFIG_GET = 208,
    MSG_ADMIN_CONFIG_SET = 209,
    
    MSG_MAX = 255
} message_type_t;

/* Programming languages */
typedef enum {
    LANGUAGE_UNKNOWN = 0,
    LANGUAGE_C = 1,
    LANGUAGE_CPP = 2,
    LANGUAGE_JAVA = 3,
    LANGUAGE_PYTHON = 4,
    LANGUAGE_JAVASCRIPT = 5,
    LANGUAGE_GO = 6,
    LANGUAGE_RUST = 7,
    LANGUAGE_MAX = 8
} language_t;

/* Execution modes */
typedef enum {
    EXEC_MODE_COMPILE_ONLY = 0,
    EXEC_MODE_COMPILE_AND_RUN = 1,
    EXEC_MODE_INTERPRET_ONLY = 2,
    EXEC_MODE_SYNTAX_CHECK = 3
} execution_mode_t;

/* Job status codes */
typedef enum {
    JOB_STATUS_QUEUED = 0,
    JOB_STATUS_COMPILING = 1,
    JOB_STATUS_RUNNING = 2,
    JOB_STATUS_COMPLETED = 3,
    JOB_STATUS_FAILED = 4,
    JOB_STATUS_CANCELLED = 5,
    JOB_STATUS_TIMEOUT = 6
} job_status_t;

/* Error codes */
typedef enum {
    ERROR_NONE = 0,
    ERROR_INVALID_ARGUMENT = 1,
    ERROR_PERMISSION = 2,
    ERROR_NOT_FOUND = 3,
    ERROR_QUOTA_EXCEEDED = 4,
    ERROR_MEMORY_ALLOCATION = 5,
    ERROR_INTERNAL = 6,
    ERROR_TIMEOUT = 7,
    ERROR_COMPILATION = 8,
    ERROR_EXECUTION = 9,
    ERROR_NETWORK = 10,
    ERROR_FILE_IO = 11,
    ERROR_UNSUPPORTED_LANGUAGE = 12
} error_code_t;

/* Message header structure */
typedef struct {
    uint32_t magic;           /* Protocol magic number */
    uint16_t message_type;    /* Message type */
    uint16_t flags;           /* Message flags */
    uint32_t data_length;     /* Length of data payload */
    uint32_t correlation_id;  /* Correlation ID for request/response */
    uint64_t timestamp;       /* Message timestamp */
    uint32_t checksum;        /* Header checksum */
} __attribute__((packed)) message_header_t;

/* Generic message structure */
typedef struct {
    message_header_t header;
    void *data;
} message_t;

/* Hello message payload (client/server handshake) */
typedef struct {
    uint16_t client_version_major;
    uint16_t client_version_minor;
    uint16_t client_version_patch;
    uint16_t capabilities;
    char client_name[64];
    char client_platform[32];
} __attribute__((packed)) hello_payload_t;

/* File upload start payload */
typedef struct {
    uint64_t file_size;
    uint32_t chunk_count;
    uint32_t chunk_size;
    char filename[MAX_FILENAME_SIZE];
    uint32_t file_checksum;
} __attribute__((packed)) file_upload_start_t;

/* File chunk payload */
typedef struct {
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t chunk_checksum;
    /* chunk data follows */
} __attribute__((packed)) file_chunk_t;

/* Compile request payload */
typedef struct {
    uint16_t language;
    uint16_t mode;
    uint16_t flags;
    uint16_t priority;
    char filename[MAX_FILENAME_SIZE];
    char compiler_args[MAX_COMMAND_SIZE];
    char execution_args[MAX_COMMAND_SIZE];
} __attribute__((packed)) compile_request_t;

/* Compile response payload */
typedef struct {
    uint32_t job_id;
    uint16_t status;
    uint16_t reserved;
    int32_t exit_code;
    uint32_t output_size;
    uint32_t error_size;
    uint32_t execution_time_ms;
} __attribute__((packed)) compile_response_t;

/* Job status payload */
typedef struct {
    uint32_t job_id;
    uint16_t status;
    uint16_t progress;  /* 0-100 */
    time_t start_time;
    time_t end_time;
    pid_t pid;
    char status_message[256];
} __attribute__((packed)) job_status_payload_t;

/* Error payload */
typedef struct {
    uint32_t error_code;
    uint32_t error_line;
    char error_message[MAX_ERROR_MESSAGE_SIZE];
    char error_context[256];
} __attribute__((packed)) error_payload_t;

/* Admin command structure */
typedef struct {
    uint16_t command_type;
    uint16_t flags;
    uint32_t target_id;
    char command_data[512];
} __attribute__((packed)) admin_command_t;

/* Server statistics structure */
typedef struct {
    time_t start_time;
    time_t current_time;
    uint32_t total_clients;
    uint32_t active_clients;
    uint32_t total_jobs;
    uint32_t active_jobs;
    uint32_t completed_jobs;
    uint32_t failed_jobs;
    uint64_t total_bytes_received;
    uint64_t total_bytes_sent;
    uint32_t memory_usage_kb;
    float cpu_usage_percent;
    float avg_response_time_ms;
} __attribute__((packed)) server_stats_t;

/* File information structure */
typedef struct {
    uint32_t job_id;
    uint32_t client_id;
    char filename[MAX_FILENAME_SIZE];
    size_t file_size;
    time_t creation_time;
    time_t last_access;
    time_t last_modified;
    mode_t permissions;
    bool is_temporary;
} file_info_t;

/* File manager statistics */
typedef struct {
    size_t total_files;
    size_t temporary_files;
    size_t total_size;
} file_manager_stats_t;

/* Queue statistics */
typedef struct {
    size_t total_jobs;
    size_t queued_jobs;
    size_t running_jobs;
    size_t completed_jobs;
    size_t failed_jobs;
    size_t cancelled_jobs;
} queue_stats_t;

/* Protocol utility functions */
void init_message_header(message_header_t *header, message_type_t type, 
                        uint32_t data_length, uint32_t correlation_id);
int validate_message_header(const message_header_t *header);
uint32_t calculate_header_checksum(const message_header_t *header);
void header_to_network(message_header_t *header);
void header_from_network(message_header_t *header);

/* String conversion functions */
const char* message_type_to_string(message_type_t type);
const char* language_to_string(language_t language);
const char* execution_mode_to_string(execution_mode_t mode);
const char* job_status_to_string(job_status_t status);
const char* error_code_to_string(error_code_t code);

/* Language detection */
language_t detect_language_from_extension(const char *filename);
bool is_language_supported(language_t language);

/* Validation functions */
bool is_valid_filename(const char *filename);
bool is_valid_message_type(message_type_t type);
bool is_valid_language(language_t language);
bool is_valid_execution_mode(execution_mode_t mode);

/* Correlation ID generation */
uint32_t generate_correlation_id(void);

/* Message flags */
#define MSG_FLAG_COMPRESSED  0x0001
#define MSG_FLAG_ENCRYPTED   0x0002
#define MSG_FLAG_URGENT      0x0004
#define MSG_FLAG_PARTIAL     0x0008

/* Capability flags */
#define CAP_COMPRESSION      0x0001
#define CAP_ENCRYPTION       0x0002
#define CAP_FILE_TRANSFER    0x0004
#define CAP_ASYNC_EXECUTION  0x0008

/* Admin command types */
#define ADMIN_CMD_LIST_CLIENTS      1
#define ADMIN_CMD_LIST_JOBS         2
#define ADMIN_CMD_SERVER_STATS      3
#define ADMIN_CMD_DISCONNECT_CLIENT 4
#define ADMIN_CMD_KILL_JOB          5
#define ADMIN_CMD_SERVER_SHUTDOWN   6
#define ADMIN_CMD_CONFIG_LIST       7
#define ADMIN_CMD_CONFIG_GET        8
#define ADMIN_CMD_CONFIG_SET        9

/* Helper macros */
#define UNUSED __attribute__((unused))

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */