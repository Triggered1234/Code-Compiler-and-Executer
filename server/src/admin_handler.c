/**
 * @file admin_handler.c
 * @brief Admin connection handler for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#include "server.h"
#include "protocol.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <time.h>

/* Admin session structure */
typedef struct {
    int socket_fd;
    time_t connect_time;
    time_t last_activity;
    char session_id[64];
    bool authenticated;
    uint32_t commands_executed;
    uint32_t correlation_id;
} admin_session_t;

static admin_session_t g_admin_session = {-1, 0, 0, "", false, 0, 0};

/* Function prototypes */
static int handle_admin_connection(int client_fd);
static int handle_admin_message(int client_fd, const message_t* msg);
static int send_admin_response(int client_fd, message_type_t type, const void* data, size_t data_size, uint32_t correlation_id);
static int handle_list_clients_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id);
static int handle_list_jobs_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id);
static int handle_server_stats_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id);
static int handle_disconnect_client_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id);
static int handle_kill_job_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id);
static int handle_server_shutdown_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id);
static int handle_config_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id);
static void cleanup_admin_session(void);
static bool is_admin_session_expired(void);
static int send_error_response(int client_fd, uint32_t error_code, const char* error_message, uint32_t correlation_id);

/**
 * @brief Admin thread handler - main entry point
 */
void* admin_thread_handler(void* arg) {
    server_context_t* server = (server_context_t*)arg;
    struct pollfd poll_fd;
    int client_fd = -1;
    
    LOG_INFO("Admin thread started");
    
    poll_fd.fd = server->unix_socket;
    poll_fd.events = POLLIN;
    
    while (!server->shutdown_requested && !g_shutdown_requested) {
        // Poll for incoming connections with timeout
        int poll_result = poll(&poll_fd, 1, 1000); // 1 second timeout
        
        if (poll_result < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("Admin poll error: %s", strerror(errno));
            break;
        }
        
        if (poll_result == 0) {
            // Timeout - check for expired admin session
            if (client_fd != -1 && is_admin_session_expired()) {
                LOG_INFO("Admin session expired, disconnecting");
                close(client_fd);
                client_fd = -1;
                cleanup_admin_session();
            }
            continue;
        }
        
        // Check if we have a pending connection
        if (poll_fd.revents & POLLIN) {
            if (client_fd == -1) {
                // Accept new admin connection
                struct sockaddr_un client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                client_fd = accept(server->unix_socket, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    LOG_ERROR("Failed to accept admin connection: %s", strerror(errno));
                    continue;
                }
                
                LOG_INFO("Admin client connected on fd %d", client_fd);
                
                // Initialize admin session
                g_admin_session.socket_fd = client_fd;
                g_admin_session.connect_time = time(NULL);
                g_admin_session.last_activity = g_admin_session.connect_time;
                g_admin_session.authenticated = false;
                g_admin_session.commands_executed = 0;
                snprintf(g_admin_session.session_id, sizeof(g_admin_session.session_id), 
                        "admin_%ld_%d", (long)g_admin_session.connect_time, client_fd);
            } else {
                // Handle existing connection
                if (handle_admin_connection(client_fd) != 0) {
                    LOG_INFO("Admin client disconnected");
                    close(client_fd);
                    client_fd = -1;
                    cleanup_admin_session();
                }
            }
        }
        
        // Check for connection errors
        if (poll_fd.revents & (POLLHUP | POLLERR)) {
            if (client_fd != -1) {
                LOG_INFO("Admin connection closed");
                close(client_fd);
                client_fd = -1;
                cleanup_admin_session();
            }
        }
    }
    
    // Cleanup
    if (client_fd != -1) {
        close(client_fd);
        cleanup_admin_session();
    }
    
    LOG_INFO("Admin thread stopped");
    return NULL;
}

/**
 * @brief Handle admin connection messages
 */
static int handle_admin_connection(int client_fd) {
    message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    // Receive message header
    ssize_t received = recv(client_fd, &msg.header, sizeof(msg.header), MSG_DONTWAIT);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // No data available
        }
        LOG_ERROR("Failed to receive admin message header: %s", strerror(errno));
        return -1;
    }
    
    if (received == 0) {
        LOG_INFO("Admin client disconnected");
        return -1;
    }
    
    if (received != sizeof(msg.header)) {
        LOG_ERROR("Incomplete admin message header received");
        return -1;
    }
    
    // Convert from network byte order
    header_from_network(&msg.header);
    
    // Validate header
    if (validate_message_header(&msg.header) != 0) {
        LOG_ERROR("Invalid admin message header");
        return -1;
    }
    
    // Receive message data if present
    if (msg.header.data_length > 0) {
        if (msg.header.data_length > MAX_MESSAGE_SIZE) {
            LOG_ERROR("Admin message too large: %u bytes", msg.header.data_length);
            return -1;
        }
        
        msg.data = malloc(msg.header.data_length);
        if (!msg.data) {
            LOG_ERROR("Failed to allocate memory for admin message data");
            return -1;
        }
        
        received = recv(client_fd, msg.data, msg.header.data_length, 0);
        if (received != (ssize_t)msg.header.data_length) {
            LOG_ERROR("Failed to receive admin message data");
            free(msg.data);
            return -1;
        }
    }
    
    // Update activity timestamp
    g_admin_session.last_activity = time(NULL);
    
    // Handle the message
    int result = handle_admin_message(client_fd, &msg);
    
    // Cleanup
    if (msg.data) {
        free(msg.data);
    }
    
    return result;
}

/**
 * @brief Handle specific admin message types
 */
static int handle_admin_message(int client_fd, const message_t* msg) {
    LOG_DEBUG("Handling admin message type: %s", message_type_to_string(msg->header.message_type));
    
    switch (msg->header.message_type) {
        case MSG_ADMIN_CONNECT:
            // Admin authentication/handshake
            g_admin_session.authenticated = true;
            g_admin_session.correlation_id = msg->header.correlation_id;
            LOG_INFO("Admin session authenticated: %s", g_admin_session.session_id);
            return send_admin_response(client_fd, MSG_ACK, NULL, 0, msg->header.correlation_id);
            
        case MSG_ADMIN_DISCONNECT:
            LOG_INFO("Admin requested disconnect");
            send_admin_response(client_fd, MSG_ACK, NULL, 0, msg->header.correlation_id);
            return -1; // Signal disconnection
            
        case MSG_ADMIN_LIST_CLIENTS: {
            if (!g_admin_session.authenticated) {
                return send_error_response(client_fd, ERROR_PERMISSION, "Not authenticated", msg->header.correlation_id);
            }
            const admin_command_t* cmd = (const admin_command_t*)msg->data;
            return handle_list_clients_command(client_fd, cmd, msg->header.correlation_id);
        }
        
        case MSG_ADMIN_LIST_JOBS: {
            if (!g_admin_session.authenticated) {
                return send_error_response(client_fd, ERROR_PERMISSION, "Not authenticated", msg->header.correlation_id);
            }
            const admin_command_t* cmd = (const admin_command_t*)msg->data;
            return handle_list_jobs_command(client_fd, cmd, msg->header.correlation_id);
        }
        
        case MSG_ADMIN_SERVER_STATS: {
            if (!g_admin_session.authenticated) {
                return send_error_response(client_fd, ERROR_PERMISSION, "Not authenticated", msg->header.correlation_id);
            }
            const admin_command_t* cmd = (const admin_command_t*)msg->data;
            return handle_server_stats_command(client_fd, cmd, msg->header.correlation_id);
        }
        
        case MSG_ADMIN_DISCONNECT_CLIENT: {
            if (!g_admin_session.authenticated) {
                return send_error_response(client_fd, ERROR_PERMISSION, "Not authenticated", msg->header.correlation_id);
            }
            const admin_command_t* cmd = (const admin_command_t*)msg->data;
            return handle_disconnect_client_command(client_fd, cmd, msg->header.correlation_id);
        }
        
        case MSG_ADMIN_KILL_JOB: {
            if (!g_admin_session.authenticated) {
                return send_error_response(client_fd, ERROR_PERMISSION, "Not authenticated", msg->header.correlation_id);
            }
            const admin_command_t* cmd = (const admin_command_t*)msg->data;
            return handle_kill_job_command(client_fd, cmd, msg->header.correlation_id);
        }
        
        case MSG_ADMIN_SERVER_SHUTDOWN: {
            if (!g_admin_session.authenticated) {
                return send_error_response(client_fd, ERROR_PERMISSION, "Not authenticated", msg->header.correlation_id);
            }
            const admin_command_t* cmd = (const admin_command_t*)msg->data;
            return handle_server_shutdown_command(client_fd, cmd, msg->header.correlation_id);
        }
        
        case MSG_ADMIN_CONFIG_GET:
        case MSG_ADMIN_CONFIG_SET: {
            if (!g_admin_session.authenticated) {
                return send_error_response(client_fd, ERROR_PERMISSION, "Not authenticated", msg->header.correlation_id);
            }
            const admin_command_t* cmd = (const admin_command_t*)msg->data;
            return handle_config_command(client_fd, cmd, msg->header.correlation_id);
        }
        
        default:
            LOG_WARNING("Unknown admin message type: %d", msg->header.message_type);
            return send_error_response(client_fd, ERROR_INVALID_ARGUMENT, "Unknown command", msg->header.correlation_id);
    }
}

/**
 * @brief Handle list clients command
 */
static int handle_list_clients_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id) {
    UNUSED(cmd); // May be used for filtering in the future
    
    // Prepare client list response
    char* response_data = NULL;
    size_t response_size = 0;
    
    // Get client list from server context
    pthread_mutex_lock(&g_server.clients_mutex);
    
    // Count clients
    int client_count = 0;
    client_info_t* client = g_server.clients_head;
    while (client) {
        client_count++;
        client = client->next;
    }
    
    // Build response
    size_t buffer_size = client_count * 512 + 1024; // Rough estimate
    response_data = malloc(buffer_size);
    if (!response_data) {
        pthread_mutex_unlock(&g_server.clients_mutex);
        return send_error_response(client_fd, ERROR_MEMORY_ALLOCATION, "Out of memory", correlation_id);
    }
    
    char* ptr = response_data;
    size_t remaining = buffer_size;
    
    // Add header
    int written = snprintf(ptr, remaining, "Active Clients: %d\n\n", client_count);
    ptr += written;
    remaining -= written;
    
    // Add client information
    client = g_server.clients_head;
    while (client && remaining > 0) {
        time_t now = time(NULL);
        int duration = (int)(now - client->connect_time);
        
        written = snprintf(ptr, remaining,
            "ID: %u | IP: %s:%d | State: %s | Connected: %ds | Jobs: %u | Sent: %lu | Recv: %lu\n",
            client->client_id,
            client->client_ip,
            client->client_port,
            client_state_to_string(client->state),
            duration,
            client->active_jobs,
            (unsigned long)client->bytes_sent,
            (unsigned long)client->bytes_received
        );
        
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        } else {
            break; // Buffer full
        }
        
        client = client->next;
    }
    
    response_size = ptr - response_data;
    
    pthread_mutex_unlock(&g_server.clients_mutex);
    
    // Send response
    int result = send_admin_response(client_fd, MSG_ADMIN_LIST_CLIENTS, response_data, response_size, correlation_id);
    
    free(response_data);
    g_admin_session.commands_executed++;
    
    return result;
}

/**
 * @brief Handle list jobs command
 */
static int handle_list_jobs_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id) {
    UNUSED(cmd); // May be used for filtering in the future
    
    char* response_data = NULL;
    size_t response_size = 0;
    
    // Get job list from server context
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    // Count jobs
    int job_count = g_server.job_queue.count;
    
    // Build response
    size_t buffer_size = job_count * 256 + 512;
    response_data = malloc(buffer_size);
    if (!response_data) {
        pthread_mutex_unlock(&g_server.jobs_mutex);
        return send_error_response(client_fd, ERROR_MEMORY_ALLOCATION, "Out of memory", correlation_id);
    }
    
    char* ptr = response_data;
    size_t remaining = buffer_size;
    
    // Add header
    int written = snprintf(ptr, remaining, "Active Jobs: %d\n\n", job_count);
    ptr += written;
    remaining -= written;
    
    // Add job information
    job_node_t* job_node = g_server.job_queue.head;
    while (job_node && remaining > 0) {
        const job_info_t* job = &job_node->job;
        time_t now = time(NULL);
        int duration = (int)(now - job->submit_time);
        
        written = snprintf(ptr, remaining,
            "Job: %u | Client: %u | Lang: %s | State: %s | Time: %ds | PID: %d | File: %s\n",
            job->job_id,
            job->client_id,
            language_to_string(job->language),
            job_state_to_string(job->state),
            duration,
            (int)job->process_id,
            job->source_file
        );
        
        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
        } else {
            break;
        }
        
        job_node = job_node->next;
    }
    
    response_size = ptr - response_data;
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    // Send response
    int result = send_admin_response(client_fd, MSG_ADMIN_LIST_JOBS, response_data, response_size, correlation_id);
    
    free(response_data);
    g_admin_session.commands_executed++;
    
    return result;
}

/**
 * @brief Handle server stats command
 */
static int handle_server_stats_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id) {
    UNUSED(cmd);
    
    // Get server statistics
    pthread_mutex_lock(&g_server.stats_mutex);
    server_stats_t stats = g_server.stats;
    stats.current_time = time(NULL);
    pthread_mutex_unlock(&g_server.stats_mutex);
    
    // Send stats structure
    int result = send_admin_response(client_fd, MSG_ADMIN_SERVER_STATS, &stats, sizeof(stats), correlation_id);
    
    g_admin_session.commands_executed++;
    return result;
}

/**
 * @brief Handle disconnect client command
 */
static int handle_disconnect_client_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id) {
    if (!cmd) {
        return send_error_response(client_fd, ERROR_INVALID_ARGUMENT, "Missing command data", correlation_id);
    }
    
    uint32_t client_id = cmd->target_id;
    bool force = (cmd->flags & 1) != 0;
    
    LOG_INFO("Admin requested disconnect of client %u (force: %s)", client_id, force ? "yes" : "no");
    
    // Find and disconnect the client
    pthread_mutex_lock(&g_server.clients_mutex);
    
    client_info_t* client = g_server.clients_head;
    bool found = false;
    
    while (client) {
        if (client->client_id == client_id) {
            found = true;
            
            if (force) {
                // Force close the socket
                close(client->socket_fd);
                client->socket_fd = -1;
                LOG_INFO("Forcefully disconnected client %u", client_id);
            } else {
                // Send graceful disconnect message
                // This would be implemented in client_handler
                LOG_INFO("Gracefully disconnecting client %u", client_id);
            }
            
            // Mark client for removal
            client->state = CLIENT_STATE_DISCONNECTING;
            break;
        }
        client = client->next;
    }
    
    pthread_mutex_unlock(&g_server.clients_mutex);
    
    if (!found) {
        return send_error_response(client_fd, ERROR_NOT_FOUND, "Client not found", correlation_id);
    }
    
    // Update statistics
    pthread_mutex_lock(&g_server.stats_mutex);
    if (g_server.stats.active_clients > 0) {
        g_server.stats.active_clients--;
    }
    pthread_mutex_unlock(&g_server.stats_mutex);
    
    g_admin_session.commands_executed++;
    return send_admin_response(client_fd, MSG_ACK, NULL, 0, correlation_id);
}

/**
 * @brief Handle kill job command
 */
static int handle_kill_job_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id) {
    if (!cmd) {
        return send_error_response(client_fd, ERROR_INVALID_ARGUMENT, "Missing command data", correlation_id);
    }
    
    uint32_t job_id = cmd->target_id;
    bool force = (cmd->flags & 1) != 0;
    
    LOG_INFO("Admin requested kill of job %u (force: %s)", job_id, force ? "yes" : "no");
    
    // Find and kill the job
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    job_node_t* job_node = g_server.job_queue.head;
    bool found = false;
    
    while (job_node) {
        if (job_node->job.job_id == job_id) {
            found = true;
            
            // Kill the process if running
            if (job_node->job.process_id > 0) {
                int signal = force ? SIGKILL : SIGTERM;
                if (kill(job_node->job.process_id, signal) == 0) {
                    LOG_INFO("Sent signal %d to job %u (PID: %d)", 
                            signal, job_id, (int)job_node->job.process_id);
                } else {
                    LOG_WARNING("Failed to send signal to job %u: %s", job_id, strerror(errno));
                }
            }
            
            // Mark job as cancelled
            job_node->job.state = JOB_STATE_CANCELLED;
            break;
        }
        job_node = job_node->next;
    }
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    if (!found) {
        return send_error_response(client_fd, ERROR_NOT_FOUND, "Job not found", correlation_id);
    }
    
    g_admin_session.commands_executed++;
    return send_admin_response(client_fd, MSG_ACK, NULL, 0, correlation_id);
}

/**
 * @brief Handle server shutdown command
 */
static int handle_server_shutdown_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id) {
    if (!cmd) {
        return send_error_response(client_fd, ERROR_INVALID_ARGUMENT, "Missing command data", correlation_id);
    }
    
    bool graceful = (cmd->flags & 1) == 0;
    int delay = cmd->target_id;
    
    LOG_CRITICAL("Admin requested server shutdown (graceful: %s, delay: %d)", 
                graceful ? "yes" : "no", delay);
    
    // Send acknowledgment before shutdown
    send_admin_response(client_fd, MSG_ACK, NULL, 0, correlation_id);
    
    if (delay > 0) {
        LOG_INFO("Server shutdown scheduled in %d seconds", delay);
        sleep(delay);
    }
    
    // Signal server shutdown
    g_server.shutdown_requested = 1;
    g_shutdown_requested = 1;
    
    // If not graceful, force immediate shutdown
    if (!graceful) {
        LOG_CRITICAL("Forcing immediate shutdown");
        exit(0);
    }
    
    g_admin_session.commands_executed++;
    return -1; // Signal connection close
}

/**
 * @brief Handle configuration commands
 */
static int handle_config_command(int client_fd, const admin_command_t* cmd, uint32_t correlation_id) {
    if (!cmd) {
        return send_error_response(client_fd, ERROR_INVALID_ARGUMENT, "Missing command data", correlation_id);
    }
    
    // For now, just return a placeholder response
    // In a full implementation, this would interact with a configuration system
    
    char response[1024];
    int response_len;
    
    if (cmd->command_type == 7) { // CONFIG_LIST
        response_len = snprintf(response, sizeof(response),
            "Server Configuration:\n"
            "port=%d\n"
            "max_clients=%d\n"
            "client_timeout=%d\n"
            "admin_timeout=%d\n"
            "debug_mode=%s\n",
            g_server.config.port,
            g_server.config.max_clients,
            g_server.config.client_timeout,
            g_server.config.admin_timeout,
            g_server.config.debug_mode ? "true" : "false"
        );
    } else if (cmd->command_type == 8) { // CONFIG_GET
        response_len = snprintf(response, sizeof(response),
            "Configuration key '%s' not found", cmd->command_data);
    } else if (cmd->command_type == 9) { // CONFIG_SET
        response_len = snprintf(response, sizeof(response),
            "Configuration update not implemented yet");
    } else {
        return send_error_response(client_fd, ERROR_INVALID_ARGUMENT, "Unknown config command", correlation_id);
    }
    
    g_admin_session.commands_executed++;
    return send_admin_response(client_fd, MSG_ADMIN_CONFIG_GET, response, response_len, correlation_id);
}

/**
 * @brief Send admin response
 */
static int send_admin_response(int client_fd, message_type_t type, const void* data, size_t data_size, uint32_t correlation_id) {
    message_t response;
    init_message_header(&response.header, type, data_size, correlation_id);
    
    // Convert header to network byte order
    header_to_network(&response.header);
    
    // Send header
    ssize_t sent = send(client_fd, &response.header, sizeof(response.header), MSG_NOSIGNAL);
    if (sent != sizeof(response.header)) {
        LOG_ERROR("Failed to send admin response header: %s", strerror(errno));
        return -1;
    }
    
    // Send data if present
    if (data_size > 0 && data) {
        sent = send(client_fd, data, data_size, MSG_NOSIGNAL);
        if (sent != (ssize_t)data_size) {
            LOG_ERROR("Failed to send admin response data: %s", strerror(errno));
            return -1;
        }
    }
    
    LOG_DEBUG("Sent admin response: type=%s, size=%zu", message_type_to_string(type), data_size);
    return 0;
}

/**
 * @brief Send error response to admin client
 */
static int send_error_response(int client_fd, uint32_t error_code, const char* error_message, uint32_t correlation_id) {
    error_payload_t error_payload;
    memset(&error_payload, 0, sizeof(error_payload));
    
    error_payload.error_code = error_code;
    error_payload.error_line = 0;
    strncpy(error_payload.error_message, error_message, sizeof(error_payload.error_message) - 1);
    snprintf(error_payload.error_context, sizeof(error_payload.error_context), 
            "Admin session: %s", g_admin_session.session_id);
    
    LOG_WARNING("Sending admin error response: %s", error_message);
    
    return send_admin_response(client_fd, MSG_ERROR, &error_payload, sizeof(error_payload), correlation_id);
}

/**
 * @brief Check if admin session is expired
 */
static bool is_admin_session_expired(void) {
    if (g_admin_session.socket_fd == -1) {
        return false; // No active session
    }
    
    time_t now = time(NULL);
    time_t timeout = g_server.config.admin_timeout;
    
    return (now - g_admin_session.last_activity) > timeout;
}

/**
 * @brief Cleanup admin session
 */
static void cleanup_admin_session(void) {
    if (g_admin_session.socket_fd != -1) {
        LOG_INFO("Cleaning up admin session: %s", g_admin_session.session_id);
        
        time_t session_duration = time(NULL) - g_admin_session.connect_time;
        LOG_INFO("Admin session lasted %ld seconds, executed %u commands", 
                (long)session_duration, g_admin_session.commands_executed);
    }
    
    memset(&g_admin_session, 0, sizeof(g_admin_session));
    g_admin_session.socket_fd = -1;
}