/**
 * @file queue_manager.c
 * @brief Job queue management for Code Compiler & Executer
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
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

/* Job processor thread entry point */
void* job_processor_thread(void* arg) {
    server_context_t* server = (server_context_t*)arg;
    
    LOG_INFO("Job processor thread started");
    
    while (!server->shutdown_requested && !g_shutdown_requested) {
        job_info_t* job = get_next_job();
        
        if (!job) {
            // No jobs available, wait for signal
            pthread_mutex_lock(&server->jobs_mutex);
            if (server->job_queue.count == 0) {
                pthread_cond_wait(&server->job_queue_cond, &server->jobs_mutex);
            }
            pthread_mutex_unlock(&server->jobs_mutex);
            continue;
        }
        
        // Process the job
        process_compilation_job(job);
        
        // Update statistics
        pthread_mutex_lock(&server->stats_mutex);
        if (job->state == JOB_STATE_COMPLETED) {
            server->stats.completed_jobs++;
        } else {
            server->stats.failed_jobs++;
        }
        
        if (server->stats.active_jobs > 0) {
            server->stats.active_jobs--;
        }
        pthread_mutex_unlock(&server->stats_mutex);
    }
    
    LOG_INFO("Job processor thread stopped");
    return NULL;
}

/**
 * @brief Add job to queue
 */
uint32_t add_job(const job_info_t* job) {
    if (!job) {
        LOG_ERROR("Invalid job parameter");
        return 0;
    }
    
    job_node_t* node = malloc(sizeof(job_node_t));
    if (!node) {
        LOG_ERROR("Failed to allocate job node");
        return 0;
    }
    
    // Copy job data
    memcpy(&node->job, job, sizeof(job_info_t));
    node->next = NULL;
    
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    // Add to tail of queue (FIFO)
    if (g_server.job_queue.tail) {
        g_server.job_queue.tail->next = node;
    } else {
        g_server.job_queue.head = node;
    }
    g_server.job_queue.tail = node;
    g_server.job_queue.count++;
    
    // Update statistics
    pthread_mutex_lock(&g_server.stats_mutex);
    g_server.stats.total_jobs++;
    g_server.stats.active_jobs++;
    pthread_mutex_unlock(&g_server.stats_mutex);
    
    uint32_t job_id = node->job.job_id;
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    // Signal job processor thread
    pthread_cond_signal(&g_server.job_queue_cond);
    
    LOG_INFO("Added job %u to queue (queue size: %zu)", job_id, g_server.job_queue.count);
    
    return job_id;
}

/**
 * @brief Get next job from queue
 */
job_info_t* get_next_job(void) {
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    if (!g_server.job_queue.head) {
        pthread_mutex_unlock(&g_server.jobs_mutex);
        return NULL;
    }
    
    job_node_t* node = g_server.job_queue.head;
    g_server.job_queue.head = node->next;
    
    if (!g_server.job_queue.head) {
        g_server.job_queue.tail = NULL;
    }
    
    g_server.job_queue.count--;
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    job_info_t* job = malloc(sizeof(job_info_t));
    if (job) {
        memcpy(job, &node->job, sizeof(job_info_t));
    }
    
    free(node);
    
    return job;
}

/**
 * @brief Find job by ID
 */
job_info_t* find_job(uint32_t job_id) {
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    job_node_t* node = g_server.job_queue.head;
    while (node) {
        if (node->job.job_id == job_id) {
            pthread_mutex_unlock(&g_server.jobs_mutex);
            return &node->job;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    return NULL;
}

/**
 * @brief Update job state
 */
void update_job_state(uint32_t job_id, job_state_t state) {
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    job_node_t* node = g_server.job_queue.head;
    while (node) {
        if (node->job.job_id == job_id) {
            node->job.state = state;
            if (state == JOB_STATE_RUNNING) {
                node->job.start_time = time(NULL);
            } else if (state == JOB_STATE_COMPLETED || state == JOB_STATE_FAILED || state == JOB_STATE_CANCELLED) {
                node->job.end_time = time(NULL);
            }
            break;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    LOG_DEBUG("Updated job %u state to %s", job_id, job_state_to_string(state));
}

/**
 * @brief Cancel job
 */
void cancel_job(uint32_t job_id) {
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    job_node_t* node = g_server.job_queue.head;
    while (node) {
        if (node->job.job_id == job_id) {
            // Kill process if running
            if (node->job.process_id > 0 && node->job.state == JOB_STATE_RUNNING) {
                kill(node->job.process_id, SIGTERM);
                LOG_INFO("Sent SIGTERM to job %u (PID: %d)", job_id, (int)node->job.process_id);
            }
            
            node->job.state = JOB_STATE_CANCELLED;
            node->job.end_time = time(NULL);
            break;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    LOG_INFO("Cancelled job %u", job_id);
}

/**
 * @brief Complete job with results
 */
void complete_job(uint32_t job_id, int exit_code, const char* output_file, const char* error_file) {
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    job_node_t* node = g_server.job_queue.head;
    while (node) {
        if (node->job.job_id == job_id) {
            node->job.state = (exit_code == 0) ? JOB_STATE_COMPLETED : JOB_STATE_FAILED;
            node->job.exit_code = exit_code;
            node->job.end_time = time(NULL);
            
            if (output_file) {
                strncpy(node->job.output_file, output_file, sizeof(node->job.output_file) - 1);
            }
            if (error_file) {
                strncpy(node->job.error_file, error_file, sizeof(node->job.error_file) - 1);
            }
            
            break;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    LOG_INFO("Completed job %u with exit code %d", job_id, exit_code);
}

/**
 * @brief Process compilation job
 */
int process_compilation_job(job_info_t* job) {
    if (!job) {
        LOG_ERROR("Invalid job parameter");
        return -1;
    }
    
    LOG_INFO("Processing compilation job %u", job->job_id);
    
    // Update job state
    update_job_state(job->job_id, JOB_STATE_RUNNING);
    
    // Create compilation job structure
    compilation_job_t* comp_job = create_compilation_job(job->job_id, job->client_id);
    if (!comp_job) {
        LOG_ERROR("Failed to create compilation job");
        update_job_state(job->job_id, JOB_STATE_FAILED);
        return -1;
    }
    
    // Set up compilation job parameters
    comp_job->compiler_type = (compiler_type_t)job->language;
    comp_job->exec_mode = (execution_mode_t)job->mode;
    strncpy(comp_job->source_file, job->source_file, sizeof(comp_job->source_file) - 1);
    strncpy(comp_job->compiler_args, job->compiler_args, sizeof(comp_job->compiler_args) - 1);
    strncpy(comp_job->execution_args, job->execution_args, sizeof(comp_job->execution_args) - 1);
    
    // Generate output filenames
    char* source_name = basename(comp_job->source_file);
    char* dot = strrchr(source_name, '.');
    if (dot) *dot = '\0';
    
    snprintf(comp_job->executable_file, sizeof(comp_job->executable_file), "%s_exe", source_name);
    snprintf(comp_job->output_file, sizeof(comp_job->output_file), "%s_output.txt", source_name);
    snprintf(comp_job->error_file, sizeof(comp_job->error_file), "%s_error.txt", source_name);
    
    int result = 0;
    
    // Execute based on mode
    switch (comp_job->exec_mode) {
        case EXEC_MODE_COMPILE_ONLY:
            result = compile_source_code(comp_job);
            break;
            
        case EXEC_MODE_COMPILE_AND_RUN:
            result = compile_source_code(comp_job);
            if (result == 0) {
                result = execute_compiled_program(comp_job);
            }
            break;
            
        case EXEC_MODE_INTERPRET_ONLY:
            result = interpret_source_code(comp_job);
            break;
            
        case EXEC_MODE_SYNTAX_CHECK:
            result = syntax_check_only(comp_job);
            break;
            
        default:
            LOG_ERROR("Unknown execution mode: %d", comp_job->exec_mode);
            result = -1;
            break;
    }
    
    // Update job with results
    job->exit_code = comp_job->exec_exit_code;
    job->process_id = comp_job->exec_pid;
    job->output_size = comp_job->output_size;
    job->error_size = comp_job->error_size;
    
    strncpy(job->output_file, comp_job->output_file, sizeof(job->output_file) - 1);
    strncpy(job->error_file, comp_job->error_file, sizeof(job->error_file) - 1);
    
    // Complete the job
    if (result == 0) {
        complete_job(job->job_id, 0, comp_job->output_file, comp_job->error_file);
    } else {
        complete_job(job->job_id, comp_job->compile_exit_code != 0 ? comp_job->compile_exit_code : comp_job->exec_exit_code, 
                    NULL, comp_job->error_file);
    }
    
    // Cleanup
    destroy_compilation_job(comp_job);
    
    LOG_INFO("Finished processing job %u with result %d", job->job_id, result);
    
    return result;
}

/**
 * @brief Generate unique job ID
 */
uint32_t generate_job_id(void) {
    static uint32_t next_id = 1;
    static pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_mutex_lock(&id_mutex);
    uint32_t id = next_id++;
    if (next_id == 0) next_id = 1; // Wrap around, avoid 0
    pthread_mutex_unlock(&id_mutex);
    
    return id;
}

/**
 * @brief Get job state string
 */
const char* job_state_to_string(job_state_t state) {
    switch (state) {
        case JOB_STATE_QUEUED: return "Queued";
        case JOB_STATE_RUNNING: return "Running";
        case JOB_STATE_COMPLETED: return "Completed";
        case JOB_STATE_FAILED: return "Failed";
        case JOB_STATE_CANCELLED: return "Cancelled";
        case JOB_STATE_TIMEOUT: return "Timeout";
        default: return "Unknown";
    }
}

/**
 * @brief Cleanup completed jobs
 */
void cleanup_completed_jobs(void) {
    time_t now = time(NULL);
    const time_t MAX_COMPLETED_JOB_AGE = 3600; // 1 hour
    
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    job_node_t* node = g_server.job_queue.head;
    job_node_t* prev = NULL;
    int cleaned_count = 0;
    
    while (node) {
        job_node_t* next = node->next;
        
        // Check if job is completed and old enough to clean up
        if ((node->job.state == JOB_STATE_COMPLETED || 
             node->job.state == JOB_STATE_FAILED || 
             node->job.state == JOB_STATE_CANCELLED) &&
            (now - node->job.end_time) > MAX_COMPLETED_JOB_AGE) {
            
            // Remove from queue
            if (prev) {
                prev->next = next;
            } else {
                g_server.job_queue.head = next;
            }
            
            if (node == g_server.job_queue.tail) {
                g_server.job_queue.tail = prev;
            }
            
            g_server.job_queue.count--;
            
            LOG_DEBUG("Cleaned up completed job %u", node->job.job_id);
            free(node);
            cleaned_count++;
        } else {
            prev = node;
        }
        
        node = next;
    }
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    if (cleaned_count > 0) {
        LOG_INFO("Cleaned up %d completed jobs", cleaned_count);
    }
}

/**
 * @brief Get queue statistics
 */
queue_stats_t get_queue_stats(void) {
    queue_stats_t stats = {0};
    
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    stats.total_jobs = g_server.job_queue.count;
    
    job_node_t* node = g_server.job_queue.head;
    while (node) {
        switch (node->job.state) {
            case JOB_STATE_QUEUED:
                stats.queued_jobs++;
                break;
            case JOB_STATE_RUNNING:
                stats.running_jobs++;
                break;
            case JOB_STATE_COMPLETED:
                stats.completed_jobs++;
                break;
            case JOB_STATE_FAILED:
                stats.failed_jobs++;
                break;
            case JOB_STATE_CANCELLED:
                stats.cancelled_jobs++;
                break;
            default:
                break;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    return stats;
}

/**
 * @brief Get jobs for specific client
 */
int get_client_jobs(uint32_t client_id, job_info_t** jobs, size_t* count) {
    if (!jobs || !count) {
        return -1;
    }
    
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    // Count client jobs
    size_t client_job_count = 0;
    job_node_t* node = g_server.job_queue.head;
    while (node) {
        if (node->job.client_id == client_id) {
            client_job_count++;
        }
        node = node->next;
    }
    
    if (client_job_count == 0) {
        *jobs = NULL;
        *count = 0;
        pthread_mutex_unlock(&g_server.jobs_mutex);
        return 0;
    }
    
    // Allocate array
    *jobs = calloc(client_job_count, sizeof(job_info_t));
    if (!*jobs) {
        pthread_mutex_unlock(&g_server.jobs_mutex);
        return -1;
    }
    
    // Fill array
    size_t index = 0;
    node = g_server.job_queue.head;
    while (node && index < client_job_count) {
        if (node->job.client_id == client_id) {
            memcpy(&(*jobs)[index], &node->job, sizeof(job_info_t));
            index++;
        }
        node = node->next;
    }
    
    *count = client_job_count;
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    return 0;
}

/**
 * @brief Cancel all jobs for a client
 */
int cancel_client_jobs(uint32_t client_id) {
    int cancelled_count = 0;
    
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    job_node_t* node = g_server.job_queue.head;
    while (node) {
        if (node->job.client_id == client_id && 
            (node->job.state == JOB_STATE_QUEUED || node->job.state == JOB_STATE_RUNNING)) {
            
            // Kill process if running
            if (node->job.process_id > 0 && node->job.state == JOB_STATE_RUNNING) {
                kill(node->job.process_id, SIGTERM);
                LOG_INFO("Sent SIGTERM to job %u (PID: %d) for client %u", 
                        node->job.job_id, (int)node->job.process_id, client_id);
            }
            
            node->job.state = JOB_STATE_CANCELLED;
            node->job.end_time = time(NULL);
            cancelled_count++;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    if (cancelled_count > 0) {
        LOG_INFO("Cancelled %d jobs for client %u", cancelled_count, client_id);
    }
    
    return cancelled_count;
}

/**
 * @brief Set job priority
 */
int set_job_priority(uint32_t job_id, int priority) {
    if (priority < JOB_PRIORITY_LOW || priority > JOB_PRIORITY_HIGH) {
        LOG_ERROR("Invalid job priority: %d", priority);
        return -1;
    }
    
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    job_node_t* node = g_server.job_queue.head;
    while (node) {
        if (node->job.job_id == job_id) {
            node->job.priority = priority;
            pthread_mutex_unlock(&g_server.jobs_mutex);
            LOG_INFO("Set job %u priority to %d", job_id, priority);
            return 0;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    LOG_ERROR("Job %u not found", job_id);
    return -1;
}

/**
 * @brief Reorder queue by priority
 */
void reorder_queue_by_priority(void) {
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    if (g_server.job_queue.count <= 1) {
        pthread_mutex_unlock(&g_server.jobs_mutex);
        return;
    }
    
    // Simple bubble sort by priority (higher priority first)
    bool swapped;
    do {
        swapped = false;
        job_node_t* prev = NULL;
        job_node_t* current = g_server.job_queue.head;
        job_node_t* next = current ? current->next : NULL;
        
        while (next) {
            // Only reorder queued jobs
            if (current->job.state == JOB_STATE_QUEUED && 
                next->job.state == JOB_STATE_QUEUED &&
                current->job.priority < next->job.priority) {
                
                // Swap nodes
                if (prev) {
                    prev->next = next;
                } else {
                    g_server.job_queue.head = next;
                }
                
                current->next = next->next;
                next->next = current;
                
                if (current == g_server.job_queue.tail) {
                    g_server.job_queue.tail = next;
                } else if (next == g_server.job_queue.tail) {
                    g_server.job_queue.tail = current;
                }
                
                swapped = true;
                
                // Update pointers for next iteration
                prev = next;
                next = current->next;
            } else {
                prev = current;
                current = next;
                next = next->next;
            }
        }
    } while (swapped);
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    LOG_DEBUG("Reordered job queue by priority");
}

/**
 * @brief Get estimated wait time for a job
 */
time_t get_estimated_wait_time(uint32_t job_id) {
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    job_node_t* target_node = NULL;
    job_node_t* node = g_server.job_queue.head;
    
    // Find the target job
    while (node) {
        if (node->job.job_id == job_id) {
            target_node = node;
            break;
        }
        node = node->next;
    }
    
    if (!target_node || target_node->job.state != JOB_STATE_QUEUED) {
        pthread_mutex_unlock(&g_server.jobs_mutex);
        return 0; // Job not found or not queued
    }
    
    // Count jobs ahead in queue
    int jobs_ahead = 0;
    node = g_server.job_queue.head;
    while (node && node != target_node) {
        if (node->job.state == JOB_STATE_QUEUED || node->job.state == JOB_STATE_RUNNING) {
            jobs_ahead++;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    // Estimate based on average job time (simplified)
    const time_t AVERAGE_JOB_TIME = 30; // 30 seconds average
    return jobs_ahead * AVERAGE_JOB_TIME;
}

/**
 * @brief Pause job queue processing
 */
void pause_job_queue(void) {
    // This would be implemented with a global flag
    // For now, just log the action
    LOG_INFO("Job queue processing paused");
}

/**
 * @brief Resume job queue processing
 */
void resume_job_queue(void) {
    // Signal the job processor thread
    pthread_cond_broadcast(&g_server.job_queue_cond);
    LOG_INFO("Job queue processing resumed");
}

/**
 * @brief Get job queue summary
 */
void print_queue_summary(void) {
    queue_stats_t stats = get_queue_stats();
    
    LOG_INFO("=== Job Queue Summary ===");
    LOG_INFO("Total jobs: %zu", stats.total_jobs);
    LOG_INFO("Queued: %zu", stats.queued_jobs);
    LOG_INFO("Running: %zu", stats.running_jobs);
    LOG_INFO("Completed: %zu", stats.completed_jobs);
    LOG_INFO("Failed: %zu", stats.failed_jobs);
    LOG_INFO("Cancelled: %zu", stats.cancelled_jobs);
    LOG_INFO("========================");
}

/**
 * @brief Initialize job queue
 */
int job_queue_init(void) {
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    g_server.job_queue.head = NULL;
    g_server.job_queue.tail = NULL;
    g_server.job_queue.count = 0;
    g_server.job_queue.max_size = 10000; // Maximum queue size
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    LOG_INFO("Job queue initialized");
    return 0;
}

/**
 * @brief Cleanup job queue
 */
void job_queue_cleanup(void) {
    pthread_mutex_lock(&g_server.jobs_mutex);
    
    job_node_t* node = g_server.job_queue.head;
    while (node) {
        job_node_t* next = node->next;
        
        // Kill any running processes
        if (node->job.process_id > 0 && node->job.state == JOB_STATE_RUNNING) {
            kill(node->job.process_id, SIGKILL);
            LOG_INFO("Killed job %u (PID: %d) during cleanup", 
                    node->job.job_id, (int)node->job.process_id);
        }
        
        free(node);
        node = next;
    }
    
    g_server.job_queue.head = NULL;
    g_server.job_queue.tail = NULL;
    g_server.job_queue.count = 0;
    
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    LOG_INFO("Job queue cleanup completed");
}