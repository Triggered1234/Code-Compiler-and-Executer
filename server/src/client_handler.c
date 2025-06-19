/**
 * @file client_handler.c
 * @brief Client connection handler for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#include "server.h"
#include "protocol.h"
#include "common.h"
#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <time.h>
#include <fcntl.h>

/* Client connection pool */
#define MAX_POLL_FDS 1024
static struct pollfd g_poll_fds[MAX_POLL_FDS];
static int g_poll_count = 0;

/* Function prototypes */
static int handle_new_client_connection(int server_fd);
static int handle_client_message(client_info_t* client);
static int process_client_request(client_info_t* client, const message_t* msg);
static int handle_hello_message(client_info_t* client, const message_t* msg);
static int handle_file_upload_start(client_info_t* client, const message_t* msg);
static int handle_file_upload_chunk(client_info_t* client, const message_t* msg);
static int handle_file_upload_end(client_info_t* client, const message_t* msg);
static int handle_compile_request(client_info_t* client, const message_t* msg);
static int handle_status_request(client_info_t* client, const message_t* msg);
static int handle_result_request(client_info_t* client, const message_t* msg);
static int send_client_response(client_info_t* client, message_type_t type, const void* data, size_t data_size, uint32_t correlation_id);
static int send_client_error(client_info_t* client, uint32_t error_code, const char* error_message, uint32_t correlation_id);
static void cleanup_inactive_client(client_info_t* client);
static bool is_client_inactive(const client_info_t* client);
static int setup_client_socket(int client_fd);

/**
 * @brief Client thread handler - main entry point
 */
void* client_thread_handler(void* arg) {
    server_context_t* server = (server_context_t*)arg;
    
    LOG_INFO("Client thread started");
    
    // Initialize poll array
    memset(g_poll_fds, 0, sizeof(g_poll_fds));
    g_poll_fds[0].fd = server->inet_socket;
    g_poll_fds[0].events = POLLIN;
    g_poll_count = 1;
    
    while (!server->shutdown_requested && !g_shutdown_requested) {
        // Poll for events with timeout
        int poll_result = poll(g_poll_fds, g_poll_count, 1000); // 1 second timeout
        
        if (poll_result < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("Client poll error: %s", strerror(errno));
            break;
        }
        
        if (poll_result == 0) {
            // Timeout - check for inactive clients
            cleanup_inactive_clients();
            continue;
        }
        
        // Check server socket for new connections
        if (g_poll_fds[0].revents & POLLIN) {
            if (handle_new_client_connection(server->inet_socket) == 0) {
                poll_result--; // One event handled
            }
        }
        
        // Check client sockets for data
        for (int i = 1; i < g_poll_count && poll_result > 0; i++) {
            if (g_poll_fds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                // Find client by socket fd
                pthread_mutex_lock(&server->clients_mutex);
                client_info_t* client = server->clients_head;
                while (client && client->socket_fd != g_poll_fds[i].fd) {
                    client = client->next;
                }
                pthread_mutex_unlock(&server->clients_mutex);
                
                if (client) {
                    if (g_poll_fds[i].revents & POLLIN) {
                        // Handle client message
                        if (handle_client_message(client) != 0) {
                            // Client disconnected or error
                            cleanup_inactive_client(client);
                            // Remove from poll array
                            for (int j = i; j < g_poll_count - 1; j++) {
                                g_poll_fds[j] = g_poll_fds[j + 1];
                            }
                            g_poll_count--;
                            i--; // Adjust index
                        }
                    } else if (g_poll_fds[i].revents & (POLLHUP | POLLERR)) {
                        // Client disconnected
                        LOG_INFO("Client %u disconnected", client->client_id);
                        cleanup_inactive_client(client);
                        // Remove from poll array
                        for (int j = i; j < g_poll_count - 1; j++) {
                            g_poll_fds[j] = g_poll_fds[j + 1];
                        }
                        g_poll_count--;
                        i--; // Adjust index
                    }
                }
                
                poll_result--;
            }
        }
    }
    
    // Cleanup all clients
    pthread_mutex_lock(&server->clients_mutex);
    client_info_t* client = server->clients_head;
    while (client) {
        client_info_t* next = client->next;
        if (client->socket_fd != -1) {
            close(client->socket_fd);
        }
        free(client);
        client = next;
    }
    server->clients_head = NULL;
    pthread_mutex_unlock(&server->clients_mutex);
    
    LOG_INFO("Client thread stopped");
    return NULL;
}

/**
 * @brief Handle new client connection
 */
static int handle_new_client_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == -1) {
        LOG_ERROR("Failed to accept client connection: %s", strerror(errno));
        return -1;
    }
    
    // Check if we have space for more clients
    if (g_poll_count >= MAX_POLL_FDS) {
        LOG_WARNING("Maximum number of clients reached, rejecting connection");
        close(client_fd);
        return -1;
    }
    
    // Setup client socket
    if (setup_client_socket(client_fd) != 0) {
        close(client_fd);
        return -1;
    }
    
    // Get client IP and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr.sin_port);
    
    // Create client info
    client_info_t* client = add_client(client_fd, client_ip, client_port);
    if (!client) {
        LOG_ERROR("Failed to create client info");
        close(client_fd);
        return -1;
    }
    
    // Add to poll array
    g_poll_fds[g_poll_count].fd = client_fd;
    g_poll_fds[g_poll_count].events = POLLIN;
    g_poll_count++;
    
    LOG_INFO("New client connected: ID=%u, IP=%s:%d, FD=%d", 
             client->client_id, client_ip, client_port, client_fd);
    
    // Update statistics
    update_stats_client_connected();
    
    return 0;
}

/**
 * @brief Setup client socket options
 */
static int setup_client_socket(int client_fd) {
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = g_server.config.client_timeout;
    timeout.tv_usec = 0;
    
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1 ||
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
        LOG_WARNING("Failed to set socket timeout: %s", strerror(errno));
    }
    
    // Set socket to non-blocking mode
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags == -1 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        LOG_WARNING("Failed to set socket non-blocking: %s", strerror(errno));
    }
    
    // Set TCP_NODELAY for low latency
    int nodelay = 1;
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) == -1) {
        LOG_WARNING("Failed to set TCP_NODELAY: %s", strerror(errno));
    }
    
    return 0;
}

/**
 * @brief Handle client message
 */
static int handle_client_message(client_info_t* client) {
    message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    // Receive message header
    ssize_t received = recv(client->socket_fd, &msg.header, sizeof(msg.header), MSG_DONTWAIT);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // No data available
        }
        LOG_ERROR("Failed to receive message header from client %u: %s", 
                 client->client_id, strerror(errno));
        return -1;
    }
    
    if (received == 0) {
        LOG_INFO("Client %u disconnected", client->client_id);
        return -1;
    }
    
    if (received != sizeof(msg.header)) {
        LOG_ERROR("Incomplete message header from client %u", client->client_id);
        return -1;
    }
    
    // Convert from network byte order
    header_from_network(&msg.header);
    
    // Validate header
    if (validate_message_header(&msg.header) != 0) {
        LOG_ERROR("Invalid message header from client %u", client->client_id);
        return -1;
    }
    
    // Receive message data if present
    if (msg.header.data_length > 0) {
        if (msg.header.data_length > MAX_MESSAGE_SIZE) {
            LOG_ERROR("Message too large from client %u: %u bytes", 
                     client->client_id, msg.header.data_length);
            return -1;
        }
        
        msg.data = malloc(msg.header.data_length);
        if (!msg.data) {
            LOG_ERROR("Failed to allocate memory for message data");
            return -1;
        }
        
        received = recv(client->socket_fd, msg.data, msg.header.data_length, 0);
        if (received != (ssize_t)msg.header.data_length) {
            LOG_ERROR("Failed to receive message data from client %u", client->client_id);
            free(msg.data);
            return -1;
        }
        
        client->bytes_received += msg.header.data_length;
    }
    
    // Update activity timestamp
    client->last_activity = time(NULL);
    
    // Process the message
    int result = process_client_request(client, &msg);
    
    // Cleanup
    if (msg.data) {
        free(msg.data);
    }
    
    return result;
}

/**
 * @brief Process client request based on message type
 */
static int process_client_request(client_info_t* client, const message_t* msg) {
    LOG_DEBUG("Processing client %u request: %s", 
             client->client_id, message_type_to_string(msg->header.message_type));
    
    switch (msg->header.message_type) {
        case MSG_HELLO:
            return handle_hello_message(client, msg);
            
        case MSG_FILE_UPLOAD_START:
            return handle_file_upload_start(client, msg);
            
        case MSG_FILE_UPLOAD_CHUNK:
            return handle_file_upload_chunk(client, msg);
            
        case MSG_FILE_UPLOAD_END:
            return handle_file_upload_end(client, msg);
            
        case MSG_COMPILE_REQUEST:
            return handle_compile_request(client, msg);
            
        case MSG_STATUS_REQUEST:
            return handle_status_request(client, msg);
            
        case MSG_RESULT_REQUEST:
            return handle_result_request(client, msg);
            
        case MSG_PING:
            // Simple ping/pong
            return send_client_response(client, MSG_PONG, NULL, 0, msg->header.correlation_id);
            
        default:
            LOG_WARNING("Unknown message type from client %u: %d", 
                       client->client_id, msg->header.message_type);
            return send_client_error(client, ERROR_INVALID_ARGUMENT, 
                                   "Unknown message type", msg->header.correlation_id);
    }
}

/**
 * @brief Handle client hello message
 */
static int handle_hello_message(client_info_t* client, const message_t* msg) {
    if (msg->header.data_length < sizeof(hello_payload_t)) {
        return send_client_error(client, ERROR_INVALID_ARGUMENT, 
                               "Invalid hello payload", msg->header.correlation_id);
    }
    
    const hello_payload_t* hello = (const hello_payload_t*)msg->data;
    
    // Store client information
    strncpy(client->client_name, hello->client_name, sizeof(client->client_name) - 1);
    strncpy(client->client_platform, hello->client_platform, sizeof(client->client_platform) - 1);
    
    client->state = CLIENT_STATE_AUTHENTICATED;
    
    LOG_INFO("Client %u authenticated: %s on %s", 
             client->client_id, client->client_name, client->client_platform);
    
    // Send welcome response
    hello_payload_t response;
    memset(&response, 0, sizeof(response));
    response.client_version_major = PROJECT_VERSION_MAJOR;
    response.client_version_minor = PROJECT_VERSION_MINOR;
    response.client_version_patch = PROJECT_VERSION_PATCH;
    response.capabilities = 0; // Could indicate supported features
    strncpy(response.client_name, "Code Compiler & Executer Server", sizeof(response.client_name) - 1);
    strncpy(response.client_platform, PLATFORM_NAME, sizeof(response.client_platform) - 1);
    
    return send_client_response(client, MSG_HELLO, &response, sizeof(response), msg->header.correlation_id);
}

/**
 * @brief Handle file upload start
 */
static int handle_file_upload_start(client_info_t* client, const message_t* msg) {
    if (client->state != CLIENT_STATE_AUTHENTICATED && client->state != CLIENT_STATE_IDLE) {
        return send_client_error(client, ERROR_PERMISSION, 
                               "Not authenticated", msg->header.correlation_id);
    }
    
    if (msg->header.data_length < sizeof(file_upload_start_t)) {
        return send_client_error(client, ERROR_INVALID_ARGUMENT, 
                               "Invalid upload start payload", msg->header.correlation_id);
    }
    
    const file_upload_start_t* upload_start = (const file_upload_start_t*)msg->data;
    
    // Validate file size
    if (upload_start->file_size > g_server.config.max_file_size) {
        return send_client_error(client, ERROR_QUOTA_EXCEEDED, 
                               "File too large", msg->header.correlation_id);
    }
    
    client->state = CLIENT_STATE_UPLOADING;
    
    LOG_INFO("Client %u starting file upload: %s (%lu bytes)", 
             client->client_id, upload_start->filename, (unsigned long)upload_start->file_size);
    
    return send_client_response(client, MSG_ACK, NULL, 0, msg->header.correlation_id);
}

/**
 * @brief Handle file upload chunk
 */
static int handle_file_upload_chunk(client_info_t* client, const message_t* msg) {
    if (client->state != CLIENT_STATE_UPLOADING) {
        return send_client_error(client, ERROR_PERMISSION, 
                               "Not in upload mode", msg->header.correlation_id);
    }
    
    if (msg->header.data_length < sizeof(file_chunk_t)) {
        return send_client_error(client, ERROR_INVALID_ARGUMENT, 
                               "Invalid chunk payload", msg->header.correlation_id);
    }
    
    const file_chunk_t* chunk = (const file_chunk_t*)msg->data;
    const char* chunk_data = (const char*)msg->data + sizeof(file_chunk_t);
    size_t chunk_data_size = msg->header.data_length - sizeof(file_chunk_t);
    
    // Validate chunk
    if (chunk->chunk_size != chunk_data_size) {
        return send_client_error(client, ERROR_INVALID_ARGUMENT, 
                               "Chunk size mismatch", msg->header.correlation_id);
    }
    
    // TODO: Implement file chunk storage
    // For now, just acknowledge
    LOG_DEBUG("Client %u uploaded chunk %u (%u bytes)", 
             client->client_id, chunk->chunk_id, chunk->chunk_size);
    
    return send_client_response(client, MSG_ACK, NULL, 0, msg->header.correlation_id);
}

/**
 * @brief Handle file upload end
 */
static int handle_file_upload_end(client_info_t* client, const message_t* msg) {
    if (client->state != CLIENT_STATE_UPLOADING) {
        return send_client_error(client, ERROR_PERMISSION, 
                               "Not in upload mode", msg->header.correlation_id);
    }
    
    client->state = CLIENT_STATE_IDLE;
    
    LOG_INFO("Client %u completed file upload", client->client_id);
    
    return send_client_response(client, MSG_ACK, NULL, 0, msg->header.correlation_id);
}

/**
 * @brief Handle compile request
 */
static int handle_compile_request(client_info_t* client, const message_t* msg) {
    if (client->state != CLIENT_STATE_IDLE) {
        return send_client_error(client, ERROR_PERMISSION, 
                               "Client not ready", msg->header.correlation_id);
    }
    
    if (msg->header.data_length < sizeof(compile_request_t)) {
        return send_client_error(client, ERROR_INVALID_ARGUMENT, 
                               "Invalid compile request payload", msg->header.correlation_id);
    }
    
    const compile_request_t* compile_req = (const compile_request_t*)msg->data;
    
    // Create job info
    job_info_t job;
    memset(&job, 0, sizeof(job));
    
    job.job_id = generate_job_id();
    job.client_id = client->client_id;
    job.language = (language_t)compile_req->language;
    job.mode = (execution_mode_t)compile_req->mode;
    job.state = JOB_STATE_QUEUED;
    job.submit_time = time(NULL);
    job.priority = JOB_PRIORITY_NORMAL;
    
    strncpy(job.source_file, compile_req->filename, sizeof(job.source_file) - 1);
    strncpy(job.compiler_args, compile_req->compiler_args, sizeof(job.compiler_args) - 1);
    strncpy(job.execution_args, compile_req->execution_args, sizeof(job.execution_args) - 1);
    
    // Add job to queue
    uint32_t job_id = add_job(&job);
    if (job_id == 0) {
        return send_client_error(client, ERROR_INTERNAL, 
                               "Failed to queue job", msg->header.correlation_id);
    }
    
    client->state = CLIENT_STATE_PROCESSING;
    client->active_jobs++;
    
    LOG_INFO("Client %u submitted job %u for compilation: %s", 
             client->client_id, job_id, compile_req->filename);
    
    // Send job ID response
    compile_response_t response;
    memset(&response, 0, sizeof(response));
    response.job_id = job_id;
    response.status = JOB_STATUS_QUEUED;
    
    update_stats_job_submitted();
    
    return send_client_response(client, MSG_COMPILE_RESPONSE, &response, sizeof(response), msg->header.correlation_id);
}

/**
 * @brief Handle status request
 */
static int handle_status_request(client_info_t* client, const message_t* msg) {
    if (msg->header.data_length < sizeof(uint32_t)) {
        return send_client_error(client, ERROR_INVALID_ARGUMENT, 
                               "Invalid status request", msg->header.correlation_id);
    }
    
    uint32_t job_id = *(const uint32_t*)msg->data;
    
    // Find job
    job_info_t* job = find_job(job_id);
    if (!job) {
        return send_client_error(client, ERROR_NOT_FOUND, 
                               "Job not found", msg->header.correlation_id);
    }
    
    // Check if client owns this job
    if (job->client_id != client->client_id) {
        return send_client_error(client, ERROR_PERMISSION, 
                               "Access denied", msg->header.correlation_id);
    }
    
    // Prepare status response
    job_status_payload_t status;
    memset(&status, 0, sizeof(status));
    
    status.job_id = job->job_id;
    status.status = (uint16_t)job->state;
    status.start_time = job->start_time;
    status.end_time = job->end_time;
    status.pid = job->process_id;
    
    // Calculate progress (simplified)
    if (job->state == JOB_STATE_COMPLETED) {
        status.progress = 100;
    } else if (job->state == JOB_STATE_RUNNING) {
        status.progress = 50;
    } else {
        status.progress = 0;
    }
    
    snprintf(status.status_message, sizeof(status.status_message), 
            "Job %u: %s", job_id, job_state_to_string(job->state));
    
    LOG_DEBUG("Client %u requested status for job %u: %s", 
             client->client_id, job_id, job_state_to_string(job->state));
    
    return send_client_response(client, MSG_STATUS_RESPONSE, &status, sizeof(status), msg->header.correlation_id);
}

/**
 * @brief Handle result request
 */
static int handle_result_request(client_info_t* client, const message_t* msg) {
    if (msg->header.data_length < sizeof(uint32_t)) {
        return send_client_error(client, ERROR_INVALID_ARGUMENT, 
                               "Invalid result request", msg->header.correlation_id);
    }
    
    uint32_t job_id = *(const uint32_t*)msg->data;
    
    // Find job
    job_info_t* job = find_job(job_id);
    if (!job) {
        return send_client_error(client, ERROR_NOT_FOUND, 
                               "Job not found", msg->header.correlation_id);
    }
    
    // Check if client owns this job
    if (job->client_id != client->client_id) {
        return send_client_error(client, ERROR_PERMISSION, 
                               "Access denied", msg->header.correlation_id);
    }
    
    // Check if job is completed
    if (job->state != JOB_STATE_COMPLETED && job->state != JOB_STATE_FAILED) {
        return send_client_error(client, ERROR_PERMISSION, 
                               "Job not completed", msg->header.correlation_id);
    }
    
    // Prepare result response
    compile_response_t response;
    memset(&response, 0, sizeof(response));
    
    response.job_id = job->job_id;
    response.status = (uint16_t)job->state;
    response.exit_code = job->exit_code;
    response.output_size = job->output_size;
    response.error_size = job->error_size;
    
    if (job->end_time > job->start_time) {
        response.execution_time_ms = (uint32_t)((job->end_time - job->start_time) * 1000);
    }
    
    LOG_INFO("Client %u requested results for job %u", client->client_id, job_id);
    
    // TODO: In a complete implementation, we would also send the actual output and error data
    // This would require multiple messages or a larger buffer
    
    return send_client_response(client, MSG_RESULT_RESPONSE, &response, sizeof(response), msg->header.correlation_id);
}

/**
 * @brief Send response to client
 */
static int send_client_response(client_info_t* client, message_type_t type, const void* data, size_t data_size, uint32_t correlation_id) {
    message_t response;
    init_message_header(&response.header, type, data_size, correlation_id);
    
    // Convert header to network byte order
    header_to_network(&response.header);
    
    // Send header
    ssize_t sent = send(client->socket_fd, &response.header, sizeof(response.header), MSG_NOSIGNAL);
    if (sent != sizeof(response.header)) {
        LOG_ERROR("Failed to send response header to client %u: %s", 
                 client->client_id, strerror(errno));
        return -1;
    }
    
    client->bytes_sent += sizeof(response.header);
    
    // Send data if present
    if (data_size > 0 && data) {
        sent = send(client->socket_fd, data, data_size, MSG_NOSIGNAL);
        if (sent != (ssize_t)data_size) {
            LOG_ERROR("Failed to send response data to client %u: %s", 
                     client->client_id, strerror(errno));
            return -1;
        }
        
        client->bytes_sent += data_size;
    }
    
    LOG_DEBUG("Sent response to client %u: type=%s, size=%zu", 
             client->client_id, message_type_to_string(type), data_size);
    
    return 0;
}

/**
 * @brief Send error response to client
 */
static int send_client_error(client_info_t* client, uint32_t error_code, const char* error_message, uint32_t correlation_id) {
    error_payload_t error_payload;
    memset(&error_payload, 0, sizeof(error_payload));
    
    error_payload.error_code = error_code;
    error_payload.error_line = 0;
    strncpy(error_payload.error_message, error_message, sizeof(error_payload.error_message) - 1);
    snprintf(error_payload.error_context, sizeof(error_payload.error_context), 
            "Client %u", client->client_id);
    
    LOG_WARNING("Sending error to client %u: %s", client->client_id, error_message);
    
    return send_client_response(client, MSG_ERROR, &error_payload, sizeof(error_payload), correlation_id);
}

/**
 * @brief Check if client is inactive and should be disconnected
 */
static bool is_client_inactive(const client_info_t* client) {
    time_t now = time(NULL);
    time_t timeout = g_server.config.client_timeout;
    
    return (now - client->last_activity) > timeout;
}

/**
 * @brief Cleanup inactive client
 */
static void cleanup_inactive_client(client_info_t* client) {
    if (!client) return;
    
    LOG_INFO("Cleaning up client %u", client->client_id);
    
    // Close socket
    if (client->socket_fd != -1) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
    
    // Cancel any active jobs for this client
    pthread_mutex_lock(&g_server.jobs_mutex);
    job_node_t* job_node = g_server.job_queue.head;
    while (job_node) {
        if (job_node->job.client_id == client->client_id) {
            if (job_node->job.state == JOB_STATE_RUNNING && job_node->job.process_id > 0) {
                LOG_INFO("Cancelling job %u for disconnected client %u", 
                        job_node->job.job_id, client->client_id);
                kill(job_node->job.process_id, SIGTERM);
            }
            job_node->job.state = JOB_STATE_CANCELLED;
        }
        job_node = job_node->next;
    }
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    // Remove client from list
    remove_client(client->client_id);
    
    // Update statistics
    update_stats_client_disconnected();
}

/**
 * @brief Cleanup all inactive clients
 */
void cleanup_inactive_clients(void) {
    pthread_mutex_lock(&g_server.clients_mutex);
    
    client_info_t* client = g_server.clients_head;
    while (client) {
        client_info_t* next = client->next;
        
        if (is_client_inactive(client)) {
            LOG_INFO("Client %u inactive for %ld seconds, disconnecting", 
                    client->client_id, (long)(time(NULL) - client->last_activity));
            
            // Find and remove from poll array
            for (int i = 1; i < g_poll_count; i++) {
                if (g_poll_fds[i].fd == client->socket_fd) {
                    for (int j = i; j < g_poll_count - 1; j++) {
                        g_poll_fds[j] = g_poll_fds[j + 1];
                    }
                    g_poll_count--;
                    break;
                }
            }
            
            cleanup_inactive_client(client);
        }
        
        client = next;
    }
    
    pthread_mutex_unlock(&g_server.clients_mutex);
}