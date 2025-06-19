/**
 * @file server.c
 * @brief Code Compiler and Executor Server
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @version 1.0.0
 *
 * @details This is the main server application that handles code compilation
 * and execution requests from clients. It supports both regular clients
 * (for code submission) and admin clients (for server management).
 *
 * The server uses a multi-threaded architecture with specialized threads:
 * - Main thread: Coordinates other threads
 * - Regular client thread: Handles code compilation requests (port 8080)
 * - Admin thread: Handles administration requests (port 8081)
 * - Client handler threads: One per connected client
 *
 * @copyright This project is for educational purposes as part of the PCD
 * course.
 */

#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/** @def PORT
 * @brief Default port for regular client connections
 */
#define PORT 8080

/** @def ADMIN_PORT
 * @brief Default port for admin client connections
 */
#define ADMIN_PORT 8081

/** @def BUFFER_SIZE
 * @brief Maximum buffer size for network communications
 */
#define BUFFER_SIZE 4096

/** @def MAX_CLIENTS
 * @brief Maximum number of concurrent client connections
 */
#define MAX_CLIENTS 10

/**
 * @struct client_info_t
 * @brief Structure to hold client connection information
 *
 * @var client_info_t::socket
 * Client socket file descriptor
 * @var client_info_t::type
 * Client type ("regular" or "admin")
 */
typedef struct {
  int socket;    /**< Client socket file descriptor */
  char type[20]; /**< Client type identifier */
} client_info_t;

/** @brief Global flag to control server running state */
int server_running = 1;

/** @brief Total number of compilation attempts */
int total_compilations = 0;

/** @brief Number of successful compilations */
int successful_compilations = 0;

/** @brief Mutex for protecting statistics variables */
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Log activities to server log file
 *
 * This function writes timestamped log messages to server.log file.
 * It automatically adds timestamps and handles file operations.
 *
 * @param message The message to log
 *
 * @note The function is thread-safe but doesn't use explicit locking.
 * Multiple threads may write simultaneously, which could interleave output.
 *
 * @warning If the log file cannot be opened, the message is silently discarded.
 */
void log_activity(const char *message) {
  FILE *log_file = fopen("server.log", "a");
  if (log_file) {
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Remove newline
    fprintf(log_file, "[%s] %s\n", time_str, message);
    fclose(log_file);
  }
}

/**
 * @brief Compile and execute C source code
 *
 * This function takes C source code, writes it to a temporary file,
 * compiles it using GCC, and executes the resulting program.
 *
 * @param code Pointer to null-terminated C source code string
 * @param output Buffer to store compilation/execution output
 * @param output_size Size of the output buffer
 *
 * @return 0 on successful execution, -1 on compilation failure,
 *         or the exit code of the executed program
 *
 * @details The function performs the following steps:
 * 1. Writes source code to "temp_code.c"
 * 2. Compiles using "gcc temp_code.c -o temp_program"
 * 3. Executes with 5-second timeout using "timeout 5 ./temp_program"
 * 4. Captures both stdout and stderr
 * 5. Updates compilation statistics
 * 6. Cleans up temporary files
 *
 * @note The function uses system() calls which could be a security risk
 * in production environments. For educational purposes only.
 *
 * @warning Temporary files are created in the current directory.
 * Ensure proper permissions and disk space.
 */
int compile_and_execute(const char *code, char *output, size_t output_size) {
  char log_msg[256];

  // Write code to temporary file
  FILE *temp_file = fopen("temp_code.c", "w");
  if (!temp_file) {
    snprintf(output, output_size, "ERROR: Cannot create temporary file\n");
    return -1;
  }
  fprintf(temp_file, "%s", code);
  fclose(temp_file);

  pthread_mutex_lock(&stats_mutex);
  total_compilations++;
  pthread_mutex_unlock(&stats_mutex);

  // Compile the code
  int compile_result =
      system("gcc temp_code.c -o temp_program 2> compile_error.log");

  if (compile_result != 0) {
    // Compilation failed
    FILE *error_file = fopen("compile_error.log", "r");
    if (error_file) {
      fread(output, 1, output_size - 1, error_file);
      fclose(error_file);
    } else {
      snprintf(output, output_size, "ERROR: Compilation failed\n");
    }
    snprintf(log_msg, sizeof(log_msg), "Compilation failed");
    log_activity(log_msg);
    return -1;
  }

  // Execute the program
  FILE *exec_pipe = popen("timeout 5 ./temp_program 2>&1", "r");
  if (!exec_pipe) {
    snprintf(output, output_size, "ERROR: Cannot execute program\n");
    return -1;
  }

  size_t bytes_read = fread(output, 1, output_size - 1, exec_pipe);
  output[bytes_read] = '\0';

  int exec_result = pclose(exec_pipe);

  pthread_mutex_lock(&stats_mutex);
  if (exec_result == 0) {
    successful_compilations++;
  }
  pthread_mutex_unlock(&stats_mutex);

  // Clean up
  unlink("temp_code.c");
  unlink("temp_program");
  unlink("compile_error.log");

  snprintf(log_msg, sizeof(log_msg), "Code executed, result: %d", exec_result);
  log_activity(log_msg);

  return exec_result;
}

/**
 * @brief Handle regular client connections
 *
 * This function runs in a separate thread for each regular client connection.
 * It receives C source code from clients, compiles and executes it,
 * then sends the results back.
 *
 * @param arg Pointer to client_info_t structure containing client details
 * @return NULL when client disconnects
 *
 * @details Protocol:
 * - Receives C source code in text format
 * - Special command "QUIT" disconnects the client
 * - All other input is treated as C source code
 * - Sends compilation/execution results back to client
 *
 * @note This function is designed to be called as a pthread function.
 * The client_info_t structure is freed when the client disconnects.
 */
void *handle_client(void *arg) {
  client_info_t *client = (client_info_t *)arg;
  char buffer[BUFFER_SIZE];
  char output[BUFFER_SIZE];

  log_activity("Regular client connected");

  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(client->socket, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received <= 0) {
      break;
    }

    if (strncmp(buffer, "QUIT", 4) == 0) {
      break;
    }

    // Compile and execute the received code
    compile_and_execute(buffer, output, sizeof(output));

    // Send result back to client
    send(client->socket, output, strlen(output), 0);
  }

  log_activity("Regular client disconnected");
  close(client->socket);
  free(client);
  return NULL;
}

/**
 * @brief Handle admin client connections
 *
 * This function runs in a separate thread for each admin client connection.
 * It processes administrative commands and provides server management
 * capabilities.
 *
 * @param arg Pointer to client_info_t structure containing client details
 * @return NULL when client disconnects
 *
 * @details Supported commands:
 * - "STATUS": Returns server statistics (total/successful compilations)
 * - "LOGS": Returns contents of server.log file
 * - "SHUTDOWN": Gracefully shuts down the server
 * - "QUIT": Disconnects the admin client
 *
 * @note This function is designed to be called as a pthread function.
 * The SHUTDOWN command sets server_running to 0, causing server termination.
 */
void *handle_admin(void *arg) {
  client_info_t *client = (client_info_t *)arg;
  char buffer[BUFFER_SIZE];
  char response[BUFFER_SIZE];

  log_activity("Admin client connected");

  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(client->socket, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received <= 0) {
      break;
    }

    if (strncmp(buffer, "STATUS", 6) == 0) {
      pthread_mutex_lock(&stats_mutex);
      snprintf(response, sizeof(response),
               "Server Status:\nTotal compilations: %d\nSuccessful: "
               "%d\nFailed: %d\n",
               total_compilations, successful_compilations,
               total_compilations - successful_compilations);
      pthread_mutex_unlock(&stats_mutex);
    } else if (strncmp(buffer, "SHUTDOWN", 8) == 0) {
      snprintf(response, sizeof(response), "Server shutting down...\n");
      send(client->socket, response, strlen(response), 0);
      server_running = 0;
      break;
    } else if (strncmp(buffer, "LOGS", 4) == 0) {
      FILE *log_file = fopen("server.log", "r");
      if (log_file) {
        size_t bytes_read = fread(response, 1, sizeof(response) - 1, log_file);
        response[bytes_read] = '\0';
        fclose(log_file);
      } else {
        snprintf(response, sizeof(response), "No logs available\n");
      }
    } else if (strncmp(buffer, "QUIT", 4) == 0) {
      break;
    } else {
      snprintf(response, sizeof(response),
               "Unknown command. Available: STATUS, LOGS, SHUTDOWN, QUIT\n");
    }

    send(client->socket, response, strlen(response), 0);
  }

  log_activity("Admin client disconnected");
  close(client->socket);
  free(client);
  return NULL;
}

/**
 * @brief Regular client server thread
 *
 * This thread manages the server socket for regular client connections.
 * It listens on PORT (8080) and creates new threads for each client.
 *
 * @param arg Unused parameter (required for pthread interface)
 * @return NULL when server shuts down
 *
 * @details This function:
 * 1. Creates and configures server socket
 * 2. Binds to PORT and listens for connections
 * 3. Accepts client connections in a loop
 * 4. Creates detached threads for each client using handle_client()
 * 5. Continues until server_running becomes 0
 *
 * @note Each client connection gets its own thread for concurrent handling.
 * Threads are detached to avoid resource leaks.
 */
void *regular_server_thread(void *arg) {
  int server_fd, client_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, MAX_CLIENTS) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("Regular client server listening on port %d\n", PORT);
  log_activity("Regular client server started");

  while (server_running) {
    if ((client_socket = accept(server_fd, (struct sockaddr *)&address,
                                (socklen_t *)&addrlen)) < 0) {
      if (server_running) {
        perror("accept");
      }
      continue;
    }

    client_info_t *client = malloc(sizeof(client_info_t));
    client->socket = client_socket;
    strcpy(client->type, "regular");

    pthread_t thread;
    pthread_create(&thread, NULL, handle_client, client);
    pthread_detach(thread);
  }

  close(server_fd);
  return NULL;
}

/**
 * @brief Admin server thread
 *
 * This thread manages the server socket for admin client connections.
 * It listens on ADMIN_PORT (8081) and creates new threads for each admin.
 *
 * @param arg Unused parameter (required for pthread interface)
 * @return NULL when server shuts down
 *
 * @details This function:
 * 1. Creates and configures server socket
 * 2. Binds to ADMIN_PORT and listens for connections
 * 3. Accepts admin connections in a loop
 * 4. Creates detached threads for each admin using handle_admin()
 * 5. Continues until server_running becomes 0
 *
 * @note Admin connections are limited (usually fewer than regular clients).
 * Each admin connection gets its own thread for concurrent administration.
 */
void *admin_server_thread(void *arg) {
  int server_fd, client_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(ADMIN_PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("Admin server listening on port %d\n", ADMIN_PORT);
  log_activity("Admin server started");

  while (server_running) {
    if ((client_socket = accept(server_fd, (struct sockaddr *)&address,
                                (socklen_t *)&addrlen)) < 0) {
      if (server_running) {
        perror("accept");
      }
      continue;
    }

    client_info_t *client = malloc(sizeof(client_info_t));
    client->socket = client_socket;
    strcpy(client->type, "admin");

    pthread_t thread;
    pthread_create(&thread, NULL, handle_admin, client);
    pthread_detach(thread);
  }

  close(server_fd);
  return NULL;
}

int main(void) {
  pthread_t regular_thread, admin_thread;

  printf("Starting Code Compiler & Executor Server...\n");
  log_activity("Server starting");

  // Create directories if they don't exist
  system("mkdir -p processing outgoing");

  // Start both server threads
  pthread_create(&regular_thread, NULL, regular_server_thread, NULL);
  pthread_create(&admin_thread, NULL, admin_server_thread, NULL);

  // Wait for shutdown signal
  while (server_running) {
    sleep(1);
  }

  printf("Server shutting down...\n");
  log_activity("Server shutting down");

  pthread_cancel(regular_thread);
  pthread_cancel(admin_thread);

  return 0;
}