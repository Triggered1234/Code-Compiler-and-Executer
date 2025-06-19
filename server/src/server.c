/**
 * @file server.c
 * @brief Main server implementation for Code Compiler & Executer
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
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

/* Global server state */
server_context_t g_server;
volatile sig_atomic_t g_shutdown_requested = 0;

/* Function prototypes */
static void signal_handler(int sig);
static int parse_arguments(int argc, char *argv[]);
static int initialize_server(void);
static void cleanup_server(void);
static int create_inet_socket(int port);
static int create_unix_socket(const char *path);
static void print_usage(const char *program_name);
static void print_version(void);
static int setup_signal_handling(void);
static int create_directories(void);
static void log_server_info(void);

/**
 * @brief Signal handler for graceful shutdown
 */
static void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            LOG_INFO("Received shutdown signal (%d)", sig);
            g_shutdown_requested = 1;
            break;
        case SIGHUP:
            LOG_INFO("Received SIGHUP - reloading configuration");
            // TODO: Implement config reload
            break;
        case SIGPIPE:
            // Ignore broken pipe signals
            break;
        default:
            LOG_WARNING("Received unexpected signal: %d", sig);
            break;
    }
}

/**
 * @brief Setup signal handling
 */
static int setup_signal_handling(void) {
    struct sigaction sa;
    
    // Block signals for all threads except main
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGPIPE);
    
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        LOG_ERROR("Failed to block signals: %s", strerror(errno));
        return -1;
    }
    
    // Setup signal handler for main thread
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1 ||
        sigaction(SIGHUP, &sa, NULL) == -1) {
        LOG_ERROR("Failed to setup signal handlers: %s", strerror(errno));
        return -1;
    }
    
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    
    LOG_DEBUG("Signal handling setup completed");
    return 0;
}

/**
 * @brief Create INET socket for client connections
 */
static int create_inet_socket(int port) {
    int sockfd;
    struct sockaddr_in addr;
    int opt = 1;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        LOG_ERROR("Failed to create INET socket: %s", strerror(errno));
        return -1;
    }
    
    // Set socket options
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        LOG_WARNING("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    
    // Bind socket
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        LOG_ERROR("Failed to bind INET socket to port %d: %s", port, strerror(errno));
        close(sockfd);
        return -1;
    }
    
    if (listen(sockfd, SOCKET_BACKLOG) == -1) {
        LOG_ERROR("Failed to listen on INET socket: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    LOG_INFO("INET socket created and listening on port %d", port);
    return sockfd;
}

/**
 * @brief Create UNIX socket for admin connections
 */
static int create_unix_socket(const char *path) {
    int sockfd;
    struct sockaddr_un addr;
    
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        LOG_ERROR("Failed to create UNIX socket: %s", strerror(errno));
        return -1;
    }
    
    // Remove existing socket file
    unlink(path);
    
    // Bind socket
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        LOG_ERROR("Failed to bind UNIX socket to %s: %s", path, strerror(errno));
        close(sockfd);
        return -1;
    }
    
    if (listen(sockfd, 1) == -1) {  // Only one admin connection
        LOG_ERROR("Failed to listen on UNIX socket: %s", strerror(errno));
        close(sockfd);
        unlink(path);
        return -1;
    }
    
    // Set permissions for admin socket
    if (chmod(path, 0600) == -1) {
        LOG_WARNING("Failed to set permissions on admin socket: %s", strerror(errno));
    }
    
    LOG_INFO("UNIX socket created and listening on %s", path);
    return sockfd;
}

/**
 * @brief Create necessary directories
 */
static int create_directories(void) {
    const char *dirs[] = {
        g_server.config.processing_dir,
        g_server.config.outgoing_dir,
        g_server.config.log_dir,
        NULL
    };
    
    for (int i = 0; dirs[i]; i++) {
        if (mkdir(dirs[i], 0755) == -1 && errno != EEXIST) {
            LOG_ERROR("Failed to create directory %s: %s", dirs[i], strerror(errno));
            return -1;
        }
        LOG_DEBUG("Directory created/verified: %s", dirs[i]);
    }
    
    return 0;
}

/**
 * @brief Initialize server context and resources
 */
static int initialize_server(void) {
    // Initialize server context
    memset(&g_server, 0, sizeof(g_server));
    
    // Set default configuration
    g_server.config.port = DEFAULT_SERVER_PORT;
    strncpy(g_server.config.admin_socket_path, DEFAULT_ADMIN_SOCKET, 
            sizeof(g_server.config.admin_socket_path) - 1);
    strncpy(g_server.config.processing_dir, "./processing", 
            sizeof(g_server.config.processing_dir) - 1);
    strncpy(g_server.config.outgoing_dir, "./outgoing", 
            sizeof(g_server.config.outgoing_dir) - 1);
    strncpy(g_server.config.log_dir, "./logs", 
            sizeof(g_server.config.log_dir) - 1);
    
    g_server.config.max_clients = MAX_CLIENTS;
    g_server.config.client_timeout = CLIENT_TIMEOUT;
    g_server.config.admin_timeout = ADMIN_TIMEOUT;
    g_server.config.compile_timeout = COMPILE_TIMEOUT;
    g_server.config.debug_mode = 0;
    
    // Initialize mutexes
    if (pthread_mutex_init(&g_server.clients_mutex, NULL) != 0 ||
        pthread_mutex_init(&g_server.jobs_mutex, NULL) != 0 ||
        pthread_mutex_init(&g_server.stats_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize mutexes: %s", strerror(errno));
        return -1;
    }
    
    // Initialize condition variables
    if (pthread_cond_init(&g_server.job_queue_cond, NULL) != 0) {
        LOG_ERROR("Failed to initialize condition variables: %s", strerror(errno));
        return -1;
    }
    
    // Initialize job queue
    g_server.job_queue.head = NULL;
    g_server.job_queue.tail = NULL;
    g_server.job_queue.count = 0;
    
    // Initialize statistics
    g_server.stats.start_time = time(NULL);
    g_server.stats.total_clients = 0;
    g_server.stats.active_clients = 0;
    g_server.stats.total_jobs = 0;
    g_server.stats.completed_jobs = 0;
    g_server.stats.failed_jobs = 0;
    
    LOG_DEBUG("Server context initialized");
    return 0;
}

/**
 * @brief Create server sockets
 */
static int create_sockets(void) {
    // Create INET socket for clients
    g_server.inet_socket = create_inet_socket(g_server.config.port);
    if (g_server.inet_socket == -1) {
        return -1;
    }
    
    // Create UNIX socket for admin
    g_server.unix_socket = create_unix_socket(g_server.config.admin_socket_path);
    if (g_server.unix_socket == -1) {
        close(g_server.inet_socket);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Start worker threads
 */
static int start_threads(void) {
    int ret;
    
    // Start admin handler thread
    ret = pthread_create(&g_server.admin_thread, NULL, admin_thread_handler, &g_server);
    if (ret != 0) {
        LOG_ERROR("Failed to create admin thread: %s", strerror(ret));
        return -1;
    }
    
    // Start client handler thread
    ret = pthread_create(&g_server.client_thread, NULL, client_thread_handler, &g_server);
    if (ret != 0) {
        LOG_ERROR("Failed to create client thread: %s", strerror(ret));
        return -1;
    }
    
    // Start job processor thread
    ret = pthread_create(&g_server.processor_thread, NULL, job_processor_thread, &g_server);
    if (ret != 0) {
        LOG_ERROR("Failed to create processor thread: %s", strerror(ret));
        return -1;
    }
    
    LOG_INFO("All worker threads started successfully");
    return 0;
}

/**
 * @brief Wait for worker threads to finish
 */
static void wait_for_threads(void) {
    void *retval;
    
    LOG_INFO("Waiting for worker threads to finish...");
    
    if (g_server.admin_thread) {
        pthread_join(g_server.admin_thread, &retval);
        LOG_DEBUG("Admin thread finished");
    }
    
    if (g_server.client_thread) {
        pthread_join(g_server.client_thread, &retval);
        LOG_DEBUG("Client thread finished");
    }
    
    if (g_server.processor_thread) {
        pthread_join(g_server.processor_thread, &retval);
        LOG_DEBUG("Processor thread finished");
    }
}

/**
 * @brief Cleanup server resources
 */
static void cleanup_server(void) {
    LOG_INFO("Cleaning up server resources...");
    
    // Close sockets
    if (g_server.inet_socket != -1) {
        close(g_server.inet_socket);
        LOG_DEBUG("INET socket closed");
    }
    
    if (g_server.unix_socket != -1) {
        close(g_server.unix_socket);
        unlink(g_server.config.admin_socket_path);
        LOG_DEBUG("UNIX socket closed and unlinked");
    }
    
    // Cleanup job queue
    pthread_mutex_lock(&g_server.jobs_mutex);
    job_node_t *job = g_server.job_queue.head;
    while (job) {
        job_node_t *next = job->next;
        free(job);
        job = next;
    }
    pthread_mutex_unlock(&g_server.jobs_mutex);
    
    // Destroy mutexes and condition variables
    pthread_mutex_destroy(&g_server.clients_mutex);
    pthread_mutex_destroy(&g_server.jobs_mutex);
    pthread_mutex_destroy(&g_server.stats_mutex);
    pthread_cond_destroy(&g_server.job_queue_cond);
    
    LOG_INFO("Server cleanup completed");
}

/**
 * @brief Log server startup information
 */
static void log_server_info(void) {
    LOG_INFO("=== Code Compiler & Executer Server ===");
    LOG_INFO("Version: %s", SERVER_VERSION);
    LOG_INFO("Build: %s %s", __DATE__, __TIME__);
    LOG_INFO("Configuration:");
    LOG_INFO("  Port: %d", g_server.config.port);
    LOG_INFO("  Admin socket: %s", g_server.config.admin_socket_path);
    LOG_INFO("  Processing dir: %s", g_server.config.processing_dir);
    LOG_INFO("  Output dir: %s", g_server.config.outgoing_dir);
    LOG_INFO("  Max clients: %d", g_server.config.max_clients);
    LOG_INFO("  Debug mode: %s", g_server.config.debug_mode ? "ON" : "OFF");
    LOG_INFO("========================================");
}

/**
 * @brief Print program usage
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Code Compiler & Executer Server\n\n");
    printf("Options:\n");
    printf("  -p, --port PORT        Listen port for client connections (default: %d)\n", DEFAULT_SERVER_PORT);
    printf("  -s, --socket PATH      Admin socket path (default: %s)\n", DEFAULT_ADMIN_SOCKET);
    printf("  -c, --config FILE      Configuration file\n");
    printf("  -d, --debug            Enable debug mode\n");
    printf("  -D, --daemon           Run as daemon\n");
    printf("  -l, --log-file FILE    Log file path\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help\n");
    printf("  -V, --version          Show version\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                     # Start with default settings\n", program_name);
    printf("  %s -p 9000 -d          # Start on port 9000 with debug\n", program_name);
    printf("  %s -c server.conf -D   # Start with config file as daemon\n", program_name);
    printf("\n");
}

/**
 * @brief Print version information
 */
static void print_version(void) {
    printf("Code Compiler & Executer Server %s\n", SERVER_VERSION);
    printf("Built on %s %s\n", __DATE__, __TIME__);
    printf("Authors: Rares-Nicholas Popa & Adrian-Petru Enache\n");
}

/**
 * @brief Parse command line arguments
 */
static int parse_arguments(int argc, char *argv[]) {
    int opt;
    const char *short_options = "p:s:c:dDl:vhV";
    const struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"socket", required_argument, 0, 's'},
        {"config", required_argument, 0, 'c'},
        {"debug", no_argument, 0, 'd'},
        {"daemon", no_argument, 0, 'D'},
        {"log-file", required_argument, 0, 'l'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                g_server.config.port = atoi(optarg);
                if (g_server.config.port <= 0 || g_server.config.port > 65535) {
                    fprintf(stderr, "Error: Invalid port number: %s\n", optarg);
                    return -1;
                }
                break;
                
            case 's':
                strncpy(g_server.config.admin_socket_path, optarg, 
                       sizeof(g_server.config.admin_socket_path) - 1);
                break;
                
            case 'c':
                // TODO: Implement config file parsing
                fprintf(stderr, "Config file support not yet implemented\n");
                break;
                
            case 'd':
                g_server.config.debug_mode = 1;
                break;
                
            case 'D':
                g_server.config.daemon_mode = 1;
                break;
                
            case 'l':
                // TODO: Implement log file configuration
                fprintf(stderr, "Log file configuration not yet implemented\n");
                break;
                
            case 'v':
                g_server.config.verbose = 1;
                break;
                
            case 'h':
                print_usage(argv[0]);
                exit(0);
                
            case 'V':
                print_version();
                exit(0);
                
            default:
                print_usage(argv[0]);
                return -1;
        }
    }
    
    return 0;
}

/**
 * @brief Main server entry point
 */
int main(int argc, char *argv[]) {
    int ret = 0;
    
    // Initialize logging (basic for now)
    if (init_logging() != 0) {
        fprintf(stderr, "Failed to initialize logging\n");
        return 1;
    }
    
    // Parse command line arguments
    if (parse_arguments(argc, argv) != 0) {
        return 1;
    }
    
    // Initialize server
    if (initialize_server() != 0) {
        LOG_ERROR("Failed to initialize server");
        return 1;
    }
    
    // Create necessary directories
    if (create_directories() != 0) {
        cleanup_server();
        return 1;
    }
    
    // Setup signal handling
    if (setup_signal_handling() != 0) {
        cleanup_server();
        return 1;
    }
    
    // Create sockets
    if (create_sockets() != 0) {
        cleanup_server();
        return 1;
    }
    
    // Log server information
    log_server_info();
    
    // Start worker threads
    if (start_threads() != 0) {
        cleanup_server();
        return 1;
    }
    
    LOG_INFO("Server started successfully");
    
    // Main loop - wait for shutdown signal
    while (!g_shutdown_requested) {
        sleep(1);
        
        // Optionally print stats periodically in debug mode
        if (g_server.config.debug_mode && (time(NULL) % 30 == 0)) {
            pthread_mutex_lock(&g_server.stats_mutex);
            LOG_DEBUG("Stats: Active clients: %d, Total jobs: %d, Completed: %d, Failed: %d",
                     g_server.stats.active_clients,
                     g_server.stats.total_jobs,
                     g_server.stats.completed_jobs,
                     g_server.stats.failed_jobs);
            pthread_mutex_unlock(&g_server.stats_mutex);
        }
    }
    
    LOG_INFO("Shutdown signal received, stopping server...");
    
    // Signal all threads to stop
    g_server.shutdown_requested = 1;
    pthread_cond_broadcast(&g_server.job_queue_cond);
    
    // Wait for threads to finish
    wait_for_threads();
    
    // Cleanup
    cleanup_server();
    
    LOG_INFO("Server stopped gracefully");
    cleanup_logging();
    
    return ret;
}