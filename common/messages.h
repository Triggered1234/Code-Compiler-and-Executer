/**
 * @file messages.h
 * @brief Message handling utilities for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#ifndef MESSAGES_H
#define MESSAGES_H

#include "protocol.h"
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Message buffer management */
typedef struct {
    char *buffer;
    size_t size;
    size_t capacity;
    size_t position;
} message_buffer_t;

/* Message sending/receiving functions */
int send_message_to_socket(int sockfd, const message_t *msg);
int receive_message_from_socket(int sockfd, message_t *msg);
int send_data_chunk(int sockfd, const void *data, size_t size);
int receive_data_chunk(int sockfd, void *data, size_t size);

/* Message construction helpers */
message_t* create_hello_message(const char *client_name, const char *platform);
message_t* create_compile_request_message(language_t language, execution_mode_t mode,
                                        const char *filename, const char *compiler_args,
                                        const char *execution_args);
message_t* create_status_request_message(uint32_t job_id);
message_t* create_result_request_message(uint32_t job_id);
message_t* create_error_message(error_code_t error_code, const char *error_message);
message_t* create_ack_message(uint32_t correlation_id);

/* Admin message construction */
message_t* create_admin_connect_message(void);
message_t* create_admin_list_clients_message(void);
message_t* create_admin_list_jobs_message(void);
message_t* create_admin_server_stats_message(void);
message_t* create_admin_disconnect_client_message(uint32_t client_id);
message_t* create_admin_kill_job_message(uint32_t job_id);

/* Message destruction */
void destroy_message(message_t *msg);

/* Buffer management */
message_buffer_t* create_message_buffer(size_t initial_capacity);
void destroy_message_buffer(message_buffer_t *buffer);
int buffer_append_data(message_buffer_t *buffer, const void *data, size_t size);
int buffer_read_data(message_buffer_t *buffer, void *data, size_t size);
void buffer_reset(message_buffer_t *buffer);

/* Serialization helpers */
int serialize_compile_request(const compile_request_t *request, void **data, size_t *size);
int deserialize_compile_request(const void *data, size_t size, compile_request_t *request);
int serialize_server_stats(const server_stats_t *stats, void **data, size_t *size);
int deserialize_server_stats(const void *data, size_t size, server_stats_t *stats);

/* Network utility functions */
ssize_t send_all(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv_all(int sockfd, void *buf, size_t len, int flags);
int set_socket_timeout(int sockfd, int timeout_sec);
int set_socket_nonblocking(int sockfd);

/* Message validation */
bool validate_message_integrity(const message_t *msg);
bool validate_compile_request(const compile_request_t *request);
bool validate_file_upload_start(const file_upload_start_t *upload);

/* Protocol error handling */
const char* get_protocol_error_string(int error_code);
void log_protocol_error(const char *operation, int error_code);

/* Message logging */
void log_message_sent(const message_t *msg, int sockfd);
void log_message_received(const message_t *msg, int sockfd);
void log_message_error(const char *operation, const message_t *msg, int error_code);

/* Async message handling */
typedef struct {
    int sockfd;
    message_t *pending_msg;
    size_t bytes_sent;
    size_t bytes_received;
    bool send_complete;
    bool receive_complete;
} async_message_context_t;

async_message_context_t* create_async_context(int sockfd);
void destroy_async_context(async_message_context_t *ctx);
int async_send_message(async_message_context_t *ctx, const message_t *msg);
int async_receive_message(async_message_context_t *ctx, message_t *msg);
bool is_async_operation_complete(const async_message_context_t *ctx);

/* File transfer utilities */
int send_file_upload_start(int sockfd, const char *filename, size_t file_size);
int send_file_chunk(int sockfd, uint32_t chunk_id, const void *data, size_t size);
int send_file_upload_end(int sockfd);
int receive_file_upload_start(int sockfd, file_upload_start_t *upload_info);
int receive_file_chunk(int sockfd, file_chunk_t *chunk_info, void **data);
int receive_file_upload_end(int sockfd);

/* Compression support (if enabled) */
#ifdef ENABLE_COMPRESSION
int compress_message_data(const void *input, size_t input_size, 
                         void **output, size_t *output_size);
int decompress_message_data(const void *input, size_t input_size,
                           void **output, size_t *output_size);
#endif

/* Encryption support (if enabled) */
#ifdef ENABLE_ENCRYPTION
int encrypt_message_data(const void *input, size_t input_size,
                        void **output, size_t *output_size, const char *key);
int decrypt_message_data(const void *input, size_t input_size,
                        void **output, size_t *output_size, const char *key);
#endif

/* Message statistics */
typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t errors_count;
    double avg_send_time;
    double avg_receive_time;
} message_stats_t;

extern message_stats_t g_message_stats;

void update_message_stats_sent(size_t bytes, double time_ms);
void update_message_stats_received(size_t bytes, double time_ms);
void update_message_stats_error(void);
message_stats_t get_message_stats(void);
void reset_message_stats(void);

/* Protocol version negotiation */
typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
} protocol_version_t;

bool is_protocol_version_compatible(const protocol_version_t *client_version,
                                   const protocol_version_t *server_version);
protocol_version_t get_current_protocol_version(void);

/* Message queue for async processing */
typedef struct message_queue_node {
    message_t *message;
    struct message_queue_node *next;
} message_queue_node_t;

typedef struct {
    message_queue_node_t *head;
    message_queue_node_t *tail;
    size_t count;
    size_t max_size;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} message_queue_t;

message_queue_t* create_message_queue(size_t max_size);
void destroy_message_queue(message_queue_t *queue);
int enqueue_message(message_queue_t *queue, message_t *msg);
message_t* dequeue_message(message_queue_t *queue);
size_t get_queue_size(const message_queue_t *queue);
bool is_queue_full(const message_queue_t *queue);
bool is_queue_empty(const message_queue_t *queue);

/* Utility macros */
#define MESSAGE_ALLOC(type) ((type*)calloc(1, sizeof(type)))
#define MESSAGE_FREE(ptr) do { if(ptr) { free(ptr); (ptr) = NULL; } } while(0)

/* Error handling macros */
#define CHECK_SOCKET_ERROR(result) \
    do { \
        if ((result) == -1) { \
            log_protocol_error(__func__, errno); \
            return -1; \
        } \
    } while(0)

#define CHECK_MESSAGE_NULL(msg) \
    do { \
        if (!(msg)) { \
            log_protocol_error(__func__, EINVAL); \
            return -1; \
        } \
    } while(0)

/* Message type checking macros */
#define IS_CLIENT_MESSAGE(type) ((type) >= MSG_HELLO && (type) <= MSG_PING)
#define IS_SERVER_MESSAGE(type) ((type) >= MSG_ACK && (type) <= MSG_PONG)
#define IS_ADMIN_MESSAGE(type) ((type) >= MSG_ADMIN_CONNECT && (type) <= MSG_ADMIN_CONFIG_SET)

/* Protocol constants */
#define PROTOCOL_TIMEOUT_SEC 30
#define MAX_RETRY_ATTEMPTS 3
#define CHUNK_SIZE_DEFAULT 64*1024  /* 64KB */
#define MAX_CHUNKS_PER_FILE 1000

#ifdef __cplusplus
}
#endif

#endif /* MESSAGES_H */