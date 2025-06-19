/**
 * @file compiler_service.c
 * @brief Compiler service implementation for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#include "compiler.h"
#include "server.h"
#include "protocol.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

/* Global compiler registry */
compiler_info_t g_compilers[COMPILER_MAX];
size_t g_compiler_count = 0;
compiler_stats_t g_compiler_stats;
static pthread_mutex_t g_compiler_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Function prototypes */
static int execute_command_with_timeout(const char* command, char* output, size_t output_size, 
                                      char* error, size_t error_size, int timeout);
static char* build_compile_command(const compilation_job_t* job);
static char* build_execute_command(const compilation_job_t* job);
static int create_job_sandbox(compilation_job_t* job);
static void cleanup_job_sandbox(compilation_job_t* job);
static bool is_safe_filename(const char* filename);
static int validate_source_content(const char* source_file, compiler_type_t type);

/**
 * @brief Initialize compiler service
 */
int compiler_service_init(void) {
    LOG_INFO("Initializing compiler service");
    
    // Initialize compiler registry
    memset(g_compilers, 0, sizeof(g_compilers));
    g_compiler_count = 0;
    
    // Initialize statistics
    memset(&g_compiler_stats, 0, sizeof(g_compiler_stats));
    g_compiler_stats.last_reset_time = time(NULL);
    
    // Detect available compilers
    int detected = detect_available_compilers();
    LOG_INFO("Detected %d compilers", detected);
    
    return detected > 0 ? 0 : -1;
}

/**
 * @brief Cleanup compiler service
 */
void compiler_service_cleanup(void) {
    LOG_INFO("Cleaning up compiler service");
    
    pthread_mutex_lock(&g_compiler_mutex);
    
    // Clear compiler registry
    memset(g_compilers, 0, sizeof(g_compilers));
    g_compiler_count = 0;
    
    pthread_mutex_unlock(&g_compiler_mutex);
}

/**
 * @brief Detect available compilers on the system
 */
int detect_available_compilers(void) {
    int detected_count = 0;
    
    pthread_mutex_lock(&g_compiler_mutex);
    
    // Detect GCC (C compiler)
    if (detect_gcc_compiler(&g_compilers[g_compiler_count])) {
        g_compilers[g_compiler_count].type = COMPILER_C;
        strncpy(g_compilers[g_compiler_count].name, "gcc", sizeof(g_compilers[g_compiler_count].name) - 1);
        strncpy(g_compilers[g_compiler_count].file_extensions, ".c", sizeof(g_compilers[g_compiler_count].file_extensions) - 1);
        strncpy(g_compilers[g_compiler_count].default_args, "-Wall -Wextra -std=c99", sizeof(g_compilers[g_compiler_count].default_args) - 1);
        g_compilers[g_compiler_count].supports_debugging = true;
        g_compilers[g_compiler_count].supports_optimization = true;
        g_compilers[g_compiler_count].detection_priority = 10;
        g_compiler_count++;
        detected_count++;
        LOG_INFO("Detected C compiler: %s", g_compilers[g_compiler_count-1].executable_path);
    }
    
    // Detect G++ (C++ compiler)
    if (detect_gpp_compiler(&g_compilers[g_compiler_count])) {
        g_compilers[g_compiler_count].type = COMPILER_CPP;
        strncpy(g_compilers[g_compiler_count].name, "g++", sizeof(g_compilers[g_compiler_count].name) - 1);
        strncpy(g_compilers[g_compiler_count].file_extensions, ".cpp,.cc,.cxx", sizeof(g_compilers[g_compiler_count].file_extensions) - 1);
        strncpy(g_compilers[g_compiler_count].default_args, "-Wall -Wextra -std=c++17", sizeof(g_compilers[g_compiler_count].default_args) - 1);
        g_compilers[g_compiler_count].supports_debugging = true;
        g_compilers[g_compiler_count].supports_optimization = true;
        g_compilers[g_compiler_count].detection_priority = 10;
        g_compiler_count++;
        detected_count++;
        LOG_INFO("Detected C++ compiler: %s", g_compilers[g_compiler_count-1].executable_path);
    }
    
    // Detect Java compiler
    if (detect_javac_compiler(&g_compilers[g_compiler_count])) {
        g_compilers[g_compiler_count].type = COMPILER_JAVA;
        strncpy(g_compilers[g_compiler_count].name, "javac", sizeof(g_compilers[g_compiler_count].name) - 1);
        strncpy(g_compilers[g_compiler_count].file_extensions, ".java", sizeof(g_compilers[g_compiler_count].file_extensions) - 1);
        strncpy(g_compilers[g_compiler_count].default_args, "-cp .", sizeof(g_compilers[g_compiler_count].default_args) - 1);
        g_compilers[g_compiler_count].supports_debugging = true;
        g_compilers[g_compiler_count].supports_optimization = false;
        g_compilers[g_compiler_count].detection_priority = 8;
        g_compiler_count++;
        detected_count++;
        LOG_INFO("Detected Java compiler: %s", g_compilers[g_compiler_count-1].executable_path);
    }
    
    // Detect Python interpreter
    if (detect_python_interpreter(&g_compilers[g_compiler_count])) {
        g_compilers[g_compiler_count].type = COMPILER_PYTHON;
        strncpy(g_compilers[g_compiler_count].name, "python3", sizeof(g_compilers[g_compiler_count].name) - 1);
        strncpy(g_compilers[g_compiler_count].file_extensions, ".py", sizeof(g_compilers[g_compiler_count].file_extensions) - 1);
        strncpy(g_compilers[g_compiler_count].default_args, "-B", sizeof(g_compilers[g_compiler_count].default_args) - 1);
        g_compilers[g_compiler_count].supports_debugging = false;
        g_compilers[g_compiler_count].supports_optimization = false;
        g_compilers[g_compiler_count].detection_priority = 7;
        g_compiler_count++;
        detected_count++;
        LOG_INFO("Detected Python interpreter: %s", g_compilers[g_compiler_count-1].executable_path);
    }
    
    // Detect Node.js (JavaScript)
    if (detect_node_interpreter(&g_compilers[g_compiler_count])) {
        g_compilers[g_compiler_count].type = COMPILER_JAVASCRIPT;
        strncpy(g_compilers[g_compiler_count].name, "node", sizeof(g_compilers[g_compiler_count].name) - 1);
        strncpy(g_compilers[g_compiler_count].file_extensions, ".js", sizeof(g_compilers[g_compiler_count].file_extensions) - 1);
        strncpy(g_compilers[g_compiler_count].default_args, "", sizeof(g_compilers[g_compiler_count].default_args) - 1);
        g_compilers[g_compiler_count].supports_debugging = false;
        g_compilers[g_compiler_count].supports_optimization = false;
        g_compilers[g_compiler_count].detection_priority = 6;
        g_compiler_count++;
        detected_count++;
        LOG_INFO("Detected Node.js interpreter: %s", g_compilers[g_compiler_count-1].executable_path);
    }
    
    // Detect Go compiler
    if (detect_go_compiler(&g_compilers[g_compiler_count])) {
        g_compilers[g_compiler_count].type = COMPILER_GO;
        strncpy(g_compilers[g_compiler_count].name, "go", sizeof(g_compilers[g_compiler_count].name) - 1);
        strncpy(g_compilers[g_compiler_count].file_extensions, ".go", sizeof(g_compilers[g_compiler_count].file_extensions) - 1);
        strncpy(g_compilers[g_compiler_count].default_args, "run", sizeof(g_compilers[g_compiler_count].default_args) - 1);
        g_compilers[g_compiler_count].supports_debugging = false;
        g_compilers[g_compiler_count].supports_optimization = true;
        g_compilers[g_compiler_count].detection_priority = 5;
        g_compiler_count++;
        detected_count++;
        LOG_INFO("Detected Go compiler: %s", g_compilers[g_compiler_count-1].executable_path);
    }
    
    // Detect Rust compiler
    if (detect_rust_compiler(&g_compilers[g_compiler_count])) {
        g_compilers[g_compiler_count].type = COMPILER_RUST;
        strncpy(g_compilers[g_compiler_count].name, "rustc", sizeof(g_compilers[g_compiler_count].name) - 1);
        strncpy(g_compilers[g_compiler_count].file_extensions, ".rs", sizeof(g_compilers[g_compiler_count].file_extensions) - 1);
        strncpy(g_compilers[g_compiler_count].default_args, "--edition 2021", sizeof(g_compilers[g_compiler_count].default_args) - 1);
        g_compilers[g_compiler_count].supports_debugging = true;
        g_compilers[g_compiler_count].supports_optimization = true;
        g_compilers[g_compiler_count].detection_priority = 4;
        g_compiler_count++;
        detected_count++;
        LOG_INFO("Detected Rust compiler: %s", g_compilers[g_compiler_count-1].executable_path);
    }
    
    pthread_mutex_unlock(&g_compiler_mutex);
    
    return detected_count;
}

/**
 * @brief Detect GCC compiler
 */
COMPILER_DETECT_IMPLEMENT(gcc, "gcc", "--version")

/**
 * @brief Detect G++ compiler
 */
COMPILER_DETECT_IMPLEMENT(gpp, "g++", "--version")

/**
 * @brief Detect Java compiler
 */
COMPILER_DETECT_IMPLEMENT(javac, "javac", "-version")

/**
 * @brief Detect Python interpreter
 */
COMPILER_DETECT_IMPLEMENT(python, "python3", "--version")

/**
 * @brief Detect Node.js interpreter
 */
COMPILER_DETECT_IMPLEMENT(node, "node", "--version")

/**
 * @brief Detect Go compiler
 */
COMPILER_DETECT_IMPLEMENT(go, "go", "version")

/**
 * @brief Detect Rust compiler
 */
COMPILER_DETECT_IMPLEMENT(rust, "rustc", "--version")

/**
 * @brief Get compiler info by type
 */
compiler_info_t* get_compiler_by_type(compiler_type_t type) {
    pthread_mutex_lock(&g_compiler_mutex);
    
    for (size_t i = 0; i < g_compiler_count; i++) {
        if (g_compilers[i].type == type && g_compilers[i].available) {
            pthread_mutex_unlock(&g_compiler_mutex);
            return &g_compilers[i];
        }
    }
    
    pthread_mutex_unlock(&g_compiler_mutex);
    return NULL;
}

/**
 * @brief Detect language from filename
 */
compiler_type_t detect_language_from_filename(const char* filename) {
    if (!filename) return COMPILER_UNKNOWN;
    
    const char* ext = strrchr(filename, '.');
    if (!ext) return COMPILER_UNKNOWN;
    
    if (strcmp(ext, ".c") == 0) return COMPILER_C;
    if (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cc") == 0 || strcmp(ext, ".cxx") == 0) return COMPILER_CPP;
    if (strcmp(ext, ".java") == 0) return COMPILER_JAVA;
    if (strcmp(ext, ".py") == 0) return COMPILER_PYTHON;
    if (strcmp(ext, ".js") == 0) return COMPILER_JAVASCRIPT;
    if (strcmp(ext, ".go") == 0) return COMPILER_GO;
    if (strcmp(ext, ".rs") == 0) return COMPILER_RUST;
    
    return COMPILER_UNKNOWN;
}

/**
 * @brief Convert compiler type to string
 */
const char* compiler_type_to_string(compiler_type_t type) {
    switch (type) {
        case COMPILER_C: return "C";
        case COMPILER_CPP: return "C++";
        case COMPILER_JAVA: return "Java";
        case COMPILER_PYTHON: return "Python";
        case COMPILER_JAVASCRIPT: return "JavaScript";
        case COMPILER_GO: return "Go";
        case COMPILER_RUST: return "Rust";
        default: return "Unknown";
    }
}

/**
 * @brief Create compilation job
 */
compilation_job_t* create_compilation_job(uint32_t job_id, uint32_t client_id) {
    compilation_job_t* job = calloc(1, sizeof(compilation_job_t));
    if (!job) {
        LOG_ERROR("Failed to allocate compilation job");
        return NULL;
    }
    
    job->job_id = job_id;
    job->client_id = client_id;
    job->status = COMPILE_STATUS_PENDING;
    job->submit_time = time(NULL);
    job->compile_pid = -1;
    job->exec_pid = -1;
    job->use_sandbox = true;
    
    // Generate unique sandbox directory
    snprintf(job->sandbox_dir, sizeof(job->sandbox_dir), 
            "%s/job_%u_%lu", g_server.config.processing_dir, job_id, (unsigned long)job->submit_time);
    
    LOG_DEBUG("Created compilation job %u for client %u", job_id, client_id);
    
    return job;
}

/**
 * @brief Destroy compilation job
 */
void destroy_compilation_job(compilation_job_t* job) {
    if (!job) return;
    
    LOG_DEBUG("Destroying compilation job %u", job->job_id);
    
    // Kill any running processes
    if (job->compile_pid > 0) {
        kill(job->compile_pid, SIGKILL);
    }
    if (job->exec_pid > 0) {
        kill(job->exec_pid, SIGKILL);
    }
    
    // Cleanup sandbox
    cleanup_job_sandbox(job);
    
    free(job);
}

/**
 * @brief Compile source code
 */
int compile_source_code(compilation_job_t* job) {
    if (!job) return -1;
    
    LOG_INFO("Starting compilation for job %u", job->job_id);
    
    job->status = COMPILE_STATUS_COMPILING;
    job->compile_start_time = time(NULL);
    
    // Get compiler info
    compiler_info_t* compiler = get_compiler_by_type(job->compiler_type);
    if (!compiler) {
        LOG_ERROR("No compiler available for type %d", job->compiler_type);
        job->status = COMPILE_STATUS_FAILED;
        return -1;
    }
    
    // Create sandbox environment
    if (create_job_sandbox(job) != 0) {
        LOG_ERROR("Failed to create sandbox for job %u", job->job_id);
        job->status = COMPILE_STATUS_FAILED;
        return -1;
    }
    
    // Validate source file
    if (validate_source_content(job->source_file, job->compiler_type) != 0) {
        LOG_ERROR("Source file validation failed for job %u", job->job_id);
        job->status = COMPILE_STATUS_FAILED;
        return -1;
    }
    
    // Build compilation command
    char* compile_cmd = build_compile_command(job);
    if (!compile_cmd) {
        LOG_ERROR("Failed to build compile command for job %u", job->job_id);
        job->status = COMPILE_STATUS_FAILED;
        return -1;
    }
    
    // Execute compilation
    char compile_output[MAX_COMPILER_OUTPUT];
    char compile_errors[MAX_COMPILER_OUTPUT];
    
    int result = execute_command_with_timeout(compile_cmd, 
                                            compile_output, sizeof(compile_output),
                                            compile_errors, sizeof(compile_errors),
                                            MAX_COMPILATION_TIME);
    
    free(compile_cmd);
    
    job->compile_end_time = time(NULL);
    job->compile_exit_code = result;
    
    if (result == 0) {
        job->status = COMPILE_STATUS_COMPILED;
        LOG_INFO("Compilation successful for job %u", job->job_id);
        
        // Save compilation output
        job->output_size = strlen(compile_output);
        job->error_size = strlen(compile_errors);
        
        // Update statistics
        pthread_mutex_lock(&g_compiler_mutex);
        g_compiler_stats.successful_compilations++;
        g_compiler_stats.total_jobs++;
        double compile_time = difftime(job->compile_end_time, job->compile_start_time);
        g_compiler_stats.avg_compile_time = 
            (g_compiler_stats.avg_compile_time * (g_compiler_stats.total_jobs - 1) + compile_time) / g_compiler_stats.total_jobs;
        pthread_mutex_unlock(&g_compiler_mutex);
        
        return 0;
    } else {
        job->status = COMPILE_STATUS_FAILED;
        LOG_ERROR("Compilation failed for job %u with exit code %d", job->job_id, result);
        
        // Save error output
        job->error_size = strlen(compile_errors);
        
        // Update statistics
        pthread_mutex_lock(&g_compiler_mutex);
        g_compiler_stats.failed_compilations++;
        g_compiler_stats.total_jobs++;
        pthread_mutex_unlock(&g_compiler_mutex);
        
        return -1;
    }
}

/**
 * @brief Execute compiled program
 */
int execute_compiled_program(compilation_job_t* job) {
    if (!job) return -1;
    
    if (job->status != COMPILE_STATUS_COMPILED) {
        LOG_ERROR("Cannot execute job %u - not compiled", job->job_id);
        return -1;
    }
    
    LOG_INFO("Starting execution for job %u", job->job_id);
    
    job->status = COMPILE_STATUS_RUNNING;
    job->exec_start_time = time(NULL);
    
    // Build execution command
    char* exec_cmd = build_execute_command(job);
    if (!exec_cmd) {
        LOG_ERROR("Failed to build execute command for job %u", job->job_id);
        job->status = COMPILE_STATUS_FAILED;
        return -1;
    }
    
    // Execute program
    char exec_output[MAX_COMPILER_OUTPUT];
    char exec_errors[MAX_COMPILER_OUTPUT];
    
    int result = execute_command_with_timeout(exec_cmd,
                                            exec_output, sizeof(exec_output),
                                            exec_errors, sizeof(exec_errors),
                                            MAX_EXECUTION_TIME);
    
    free(exec_cmd);
    
    job->exec_end_time = time(NULL);
    job->exec_exit_code = result;
    
    // Save execution output
    job->output_size += strlen(exec_output);
    job->error_size += strlen(exec_errors);
    
    if (result == 0) {
        job->status = COMPILE_STATUS_COMPLETED;
        LOG_INFO("Execution successful for job %u", job->job_id);
        
        // Update statistics
        pthread_mutex_lock(&g_compiler_mutex);
        g_compiler_stats.successful_executions++;
        double exec_time = difftime(job->exec_end_time, job->exec_start_time);
        g_compiler_stats.avg_execution_time = 
            (g_compiler_stats.avg_execution_time * g_compiler_stats.successful_executions + exec_time) / 
            (g_compiler_stats.successful_executions + 1);
        pthread_mutex_unlock(&g_compiler_mutex);
        
        return 0;
    } else {
        job->status = COMPILE_STATUS_FAILED;
        LOG_ERROR("Execution failed for job %u with exit code %d", job->job_id, result);
        
        // Update statistics
        pthread_mutex_lock(&g_compiler_mutex);
        g_compiler_stats.failed_executions++;
        pthread_mutex_unlock(&g_compiler_mutex);
        
        return -1;
    }
}

/**
 * @brief Execute command with timeout
 */
static int execute_command_with_timeout(const char* command, char* output, size_t output_size, 
                                      char* error, size_t error_size, int timeout) {
    if (!command || !output || !error) return -1;
    
    LOG_DEBUG("Executing command: %s", command);
    
    int stdout_pipe[2], stderr_pipe[2];
    
    // Create pipes for stdout and stderr
    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        LOG_ERROR("Failed to create pipes: %s", strerror(errno));
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork: %s", strerror(errno));
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return -1;
    }
    
    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        
        // Redirect stdout and stderr
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        
        // Execute command
        execl("/bin/sh", "sh", "-c", command, (char*)NULL);
        exit(127); // execl failed
    }
    
    // Parent process
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    
    // Set up timeout
    time_t start_time = time(NULL);
    bool timed_out = false;
    
    // Read output with timeout
    fd_set read_fds;
    struct timeval tv;
    size_t output_pos = 0, error_pos = 0;
    
    while (!timed_out && (output_pos < output_size - 1 || error_pos < error_size - 1)) {
        FD_ZERO(&read_fds);
        FD_SET(stdout_pipe[0], &read_fds);
        FD_SET(stderr_pipe[0], &read_fds);
        
        int max_fd = (stdout_pipe[0] > stderr_pipe[0]) ? stdout_pipe[0] : stderr_pipe[0];
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int select_result = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (select_result == -1) {
            if (errno == EINTR) continue;
            LOG_ERROR("Select failed: %s", strerror(errno));
            break;
        }
        
        if (select_result == 0) {
            // Timeout - check if overall timeout exceeded
            if (time(NULL) - start_time >= timeout) {
                timed_out = true;
                break;
            }
            continue;
        }
        
        // Read stdout
        if (FD_ISSET(stdout_pipe[0], &read_fds) && output_pos < output_size - 1) {
            ssize_t bytes_read = read(stdout_pipe[0], output + output_pos, output_size - output_pos - 1);
            if (bytes_read > 0) {
                output_pos += bytes_read;
            }
        }
        
        // Read stderr
        if (FD_ISSET(stderr_pipe[0], &read_fds) && error_pos < error_size - 1) {
            ssize_t bytes_read = read(stderr_pipe[0], error + error_pos, error_size - error_pos - 1);
            if (bytes_read > 0) {
                error_pos += bytes_read;
            }
        }
    }
    
    // Null-terminate strings
    output[output_pos] = '\0';
    error[error_pos] = '\0';
    
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);
    
    // Handle timeout
    if (timed_out) {
        LOG_WARNING("Command timed out, killing process %d", pid);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return 124; // Timeout exit code
    }
    
    // Wait for child process
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        LOG_ERROR("waitpid failed: %s", strerror(errno));
        return -1;
    }
    
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    
    return -1;
}

/**
 * @brief Build compilation command
 */
static char* build_compile_command(const compilation_job_t* job) {
    if (!job) return NULL;
    
    compiler_info_t* compiler = get_compiler_by_type(job->compiler_type);
    if (!compiler) return NULL;
    
    size_t cmd_size = 2048;
    char* command = malloc(cmd_size);
    if (!command) return NULL;
    
    switch (job->compiler_type) {
        case COMPILER_C:
        case COMPILER_CPP: {
            snprintf(command, cmd_size, 
                    "cd '%s' && %s %s %s -o '%s' '%s'",
                    job->sandbox_dir,
                    compiler->executable_path,
                    compiler->default_args,
                    job->compiler_args,
                    job->executable_file,
                    job->source_file);
            break;
        }
        
        case COMPILER_JAVA: {
            snprintf(command, cmd_size,
                    "cd '%s' && %s %s %s '%s'",
                    job->sandbox_dir,
                    compiler->executable_path,
                    compiler->default_args,
                    job->compiler_args,
                    job->source_file);
            break;
        }
        
        case COMPILER_GO: {
            snprintf(command, cmd_size,
                    "cd '%s' && %s build %s -o '%s' '%s'",
                    job->sandbox_dir,
                    compiler->executable_path,
                    job->compiler_args,
                    job->executable_file,
                    job->source_file);
            break;
        }
        
        case COMPILER_RUST: {
            snprintf(command, cmd_size,
                    "cd '%s' && %s %s %s -o '%s' '%s'",
                    job->sandbox_dir,
                    compiler->executable_path,
                    compiler->default_args,
                    job->compiler_args,
                    job->executable_file,
                    job->source_file);
            break;
        }
        
        default:
            free(command);
            return NULL;
    }
    
    return command;
}

/**
 * @brief Build execution command
 */
static char* build_execute_command(const compilation_job_t* job) {
    if (!job) return NULL;
    
    compiler_info_t* compiler = get_compiler_by_type(job->compiler_type);
    if (!compiler) return NULL;
    
    size_t cmd_size = 2048;
    char* command = malloc(cmd_size);
    if (!command) return NULL;
    
    switch (job->compiler_type) {
        case COMPILER_C:
        case COMPILER_CPP:
        case COMPILER_GO:
        case COMPILER_RUST: {
            snprintf(command, cmd_size,
                    "cd '%s' && timeout %d './%s' %s",
                    job->sandbox_dir,
                    MAX_EXECUTION_TIME,
                    job->executable_file,
                    job->execution_args);
            break;
        }
        
        case COMPILER_JAVA: {
            // Extract class name from filename
            char* class_name = strdup(job->source_file);
            char* dot = strrchr(class_name, '.');
            if (dot) *dot = '\0';
            
            snprintf(command, cmd_size,
                    "cd '%s' && timeout %d java %s %s",
                    job->sandbox_dir,
                    MAX_EXECUTION_TIME,
                    class_name,
                    job->execution_args);
            
            free(class_name);
            break;
        }
        
        case COMPILER_PYTHON: {
            snprintf(command, cmd_size,
                    "cd '%s' && timeout %d %s '%s' %s",
                    job->sandbox_dir,
                    MAX_EXECUTION_TIME,
                    compiler->executable_path,
                    job->source_file,
                    job->execution_args);
            break;
        }
        
        case COMPILER_JAVASCRIPT: {
            snprintf(command, cmd_size,
                    "cd '%s' && timeout %d %s '%s' %s",
                    job->sandbox_dir,
                    MAX_EXECUTION_TIME,
                    compiler->executable_path,
                    job->source_file,
                    job->execution_args);
            break;
        }
        
        default:
            free(command);
            return NULL;
    }
    
    return command;
}

/**
 * @brief Create job sandbox directory
 */
static int create_job_sandbox(compilation_job_t* job) {
    if (!job) return -1;
    
    // Create sandbox directory
    if (mkdir(job->sandbox_dir, 0755) == -1) {
        if (errno != EEXIST) {
            LOG_ERROR("Failed to create sandbox directory %s: %s", 
                     job->sandbox_dir, strerror(errno));
            return -1;
        }
    }
    
    LOG_DEBUG("Created sandbox directory: %s", job->sandbox_dir);
    return 0;
}

/**
 * @brief Cleanup job sandbox
 */
static void cleanup_job_sandbox(compilation_job_t* job) {
    if (!job || !job->sandbox_dir[0]) return;
    
    // Remove sandbox directory and all contents
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf '%s'", job->sandbox_dir);
    
    if (system(command) != 0) {
        LOG_WARNING("Failed to cleanup sandbox directory: %s", job->sandbox_dir);
    } else {
        LOG_DEBUG("Cleaned up sandbox directory: %s", job->sandbox_dir);
    }
}

/**
 * @brief Validate source file content for security
 */
static int validate_source_content(const char* source_file, compiler_type_t type) {
    UNUSED(source_file);
    UNUSED(type);
    
    // Basic validation - in a production system, this would include:
    // - Checking for dangerous system calls
    // - Validating file size
    // - Scanning for malicious patterns
    // - Checking syntax
    
    // For now, just check if file exists and is readable
    if (access(source_file, R_OK) != 0) {
        LOG_ERROR("Source file not accessible: %s", source_file);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Check if filename is safe
 */
static bool is_safe_filename(const char* filename) {
    if (!filename) return false;
    
    // Check for directory traversal attempts
    if (strstr(filename, "..") || strstr(filename, "/") || strstr(filename, "\\")) {
        return false;
    }
    
    // Check for reasonable length
    if (strlen(filename) > MAX_FILENAME_SIZE) {
        return false;
    }
    
    return true;
}

/**
 * @brief Get compiler statistics
 */
compiler_stats_t get_compiler_stats(void) {
    pthread_mutex_lock(&g_compiler_mutex);
    compiler_stats_t stats = g_compiler_stats;
    pthread_mutex_unlock(&g_compiler_mutex);
    
    return stats;
}

/**
 * @brief Reset compiler statistics
 */
void reset_compiler_stats(void) {
    pthread_mutex_lock(&g_compiler_mutex);
    memset(&g_compiler_stats, 0, sizeof(g_compiler_stats));
    g_compiler_stats.last_reset_time = time(NULL);
    pthread_mutex_unlock(&g_compiler_mutex);
    
    LOG_INFO("Compiler statistics reset");
}

/**
 * @brief Convert compile status to string
 */
const char* compile_status_to_string(compile_status_t status) {
    switch (status) {
        case COMPILE_STATUS_PENDING: return "Pending";
        case COMPILE_STATUS_COMPILING: return "Compiling";
        case COMPILE_STATUS_COMPILED: return "Compiled";
        case COMPILE_STATUS_RUNNING: return "Running";
        case COMPILE_STATUS_COMPLETED: return "Completed";
        case COMPILE_STATUS_FAILED: return "Failed";
        case COMPILE_STATUS_TIMEOUT: return "Timeout";
        case COMPILE_STATUS_CANCELLED: return "Cancelled";
        default: return "Unknown";
    }
}