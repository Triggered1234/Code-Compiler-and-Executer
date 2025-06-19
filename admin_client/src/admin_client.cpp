/**
 * @file admin_client.cpp
 * @brief Administration client for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#include "admin_client.h"
#include "admin_commands.h"
#include "protocol.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <getopt.h>
#include <termios.h>
#include <poll.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#ifdef HAVE_NCURSES
#include <ncurses.h>
#endif

using namespace std;

// Global admin client context
AdminClient g_admin_client;
volatile bool g_shutdown_requested = false;

/**
 * @brief Signal handler for graceful shutdown
 */
void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            g_shutdown_requested = true;
            cout << "\nShutdown requested. Disconnecting..." << endl;
            break;
        case SIGPIPE:
            // Ignore broken pipe
            break;
        default:
            break;
    }
}

/**
 * @brief Setup signal handling
 */
bool setup_signal_handling() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGINT, &sa, nullptr) == -1 ||
        sigaction(SIGTERM, &sa, nullptr) == -1) {
        cerr << "Failed to setup signal handlers: " << strerror(errno) << endl;
        return false;
    }
    
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    
    return true;
}

/**
 * @brief Print program usage
 */
void print_usage(const char* program_name) {
    cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    cout << "Code Compiler & Executer Administration Client\n\n";
    cout << "Options:\n";
    cout << "  -s, --socket PATH      Server admin socket path\n";
    cout << "  -c, --config FILE      Configuration file\n";
    cout << "  -b, --batch            Batch mode (non-interactive)\n";
    cout << "  -e, --execute CMD      Execute single command and exit\n";
    cout << "  -t, --timeout SEC      Connection timeout in seconds\n";
    cout << "  -v, --verbose          Verbose output\n";
    cout << "  -q, --quiet            Quiet mode\n";
    cout << "  -h, --help             Show this help\n";
    cout << "  -V, --version          Show version\n";
    cout << "\nExamples:\n";
    cout << "  " << program_name << "                    # Interactive mode\n";
    cout << "  " << program_name << " -e \"list_clients\"   # Execute single command\n";
    cout << "  " << program_name << " -b < commands.txt  # Batch mode from file\n";
    cout << endl;
}

/**
 * @brief Print version information
 */
void print_version() {
    cout << "Admin Client " << ADMIN_CLIENT_VERSION << endl;
    cout << "Built on " << __DATE__ << " " << __TIME__ << endl;
    cout << "Authors: Rares-Nicholas Popa & Adrian-Petru Enache" << endl;
}

/**
 * @brief Parse command line arguments
 */
bool parse_arguments(int argc, char* argv[], AdminClientConfig& config) {
    int opt;
    const char* short_options = "s:c:be:t:vqhV";
    const struct option long_options[] = {
        {"socket", required_argument, 0, 's'},
        {"config", required_argument, 0, 'c'},
        {"batch", no_argument, 0, 'b'},
        {"execute", required_argument, 0, 'e'},
        {"timeout", required_argument, 0, 't'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 's':
                config.socket_path = optarg;
                break;
            case 'c':
                config.config_file = optarg;
                break;
            case 'b':
                config.batch_mode = true;
                break;
            case 'e':
                config.execute_command = optarg;
                config.batch_mode = true;
                break;
            case 't':
                config.timeout = atoi(optarg);
                if (config.timeout <= 0) {
                    cerr << "Error: Invalid timeout value: " << optarg << endl;
                    return false;
                }
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'q':
                config.quiet = true;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            case 'V':
                print_version();
                exit(0);
            default:
                print_usage(argv[0]);
                return false;
        }
    }
    
    return true;
}

/**
 * @brief Connect to server admin socket
 */
bool AdminClient::connect_to_server() {
    if (config.verbose) {
        cout << "Connecting to server at " << config.socket_path << "..." << endl;
    }
    
    // Create socket
    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        cerr << "Failed to create socket: " << strerror(errno) << endl;
        return false;
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = config.timeout;
    timeout.tv_usec = 0;
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1 ||
        setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
        cerr << "Warning: Failed to set socket timeout: " << strerror(errno) << endl;
    }
    
    // Connect to server
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, config.socket_path.c_str(), sizeof(addr.sun_path) - 1);
    
    if (connect(socket_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        cerr << "Failed to connect to server: " << strerror(errno) << endl;
        close(socket_fd);
        socket_fd = -1;
        return false;
    }
    
    connected = true;
    connect_time = time(nullptr);
    
    if (config.verbose) {
        cout << "Connected to server successfully" << endl;
    }
    
    // Send admin connect message
    return send_admin_connect();
}

/**
 * @brief Disconnect from server
 */
void AdminClient::disconnect_from_server() {
    if (connected) {
        send_admin_disconnect();
        
        if (socket_fd != -1) {
            close(socket_fd);
            socket_fd = -1;
        }
        
        connected = false;
        
        if (config.verbose) {
            cout << "Disconnected from server" << endl;
        }
    }
}

/**
 * @brief Send admin connect message
 */
bool AdminClient::send_admin_connect() {
    message_t msg;
    init_message_header(&msg.header, MSG_ADMIN_CONNECT, 0, generate_correlation_id());
    
    return send_message(msg);
}

/**
 * @brief Send admin disconnect message
 */
bool AdminClient::send_admin_disconnect() {
    message_t msg;
    init_message_header(&msg.header, MSG_ADMIN_DISCONNECT, 0, generate_correlation_id());
    
    return send_message(msg);
}

/**
 * @brief Send message to server
 */
bool AdminClient::send_message(const message_t& msg) {
    if (!connected || socket_fd == -1) {
        cerr << "Not connected to server" << endl;
        return false;
    }
    
    // Convert header to network byte order
    message_header_t header = msg.header;
    header_to_network(&header);
    
    // Send header
    ssize_t sent = send(socket_fd, &header, sizeof(header), MSG_NOSIGNAL);
    if (sent != sizeof(header)) {
        cerr << "Failed to send message header: " << strerror(errno) << endl;
        return false;
    }
    
    // Send data if present
    if (msg.header.data_length > 0 && msg.data) {
        sent = send(socket_fd, msg.data, msg.header.data_length, MSG_NOSIGNAL);
        if (sent != static_cast<ssize_t>(msg.header.data_length)) {
            cerr << "Failed to send message data: " << strerror(errno) << endl;
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Receive message from server
 */
bool AdminClient::receive_message(message_t& msg) {
    if (!connected || socket_fd == -1) {
        cerr << "Not connected to server" << endl;
        return false;
    }
    
    // Receive header
    ssize_t received = recv(socket_fd, &msg.header, sizeof(msg.header), 0);
    if (received != sizeof(msg.header)) {
        if (received == 0) {
            cerr << "Server closed connection" << endl;
        } else {
            cerr << "Failed to receive message header: " << strerror(errno) << endl;
        }
        return false;
    }
    
    // Convert header from network byte order
    header_from_network(&msg.header);
    
    // Validate header
    if (validate_message_header(&msg.header) != 0) {
        cerr << "Invalid message header received" << endl;
        return false;
    }
    
    // Receive data if present
    msg.data = nullptr;
    if (msg.header.data_length > 0) {
        msg.data = malloc(msg.header.data_length);
        if (!msg.data) {
            cerr << "Failed to allocate memory for message data" << endl;
            return false;
        }
        
        received = recv(socket_fd, msg.data, msg.header.data_length, 0);
        if (received != static_cast<ssize_t>(msg.header.data_length)) {
            cerr << "Failed to receive message data: " << strerror(errno) << endl;
            free(msg.data);
            msg.data = nullptr;
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Execute admin command
 */
bool AdminClient::execute_command(const string& command_line) {
    if (command_line.empty()) {
        return true;
    }
    
    // Parse command and arguments
    vector<string> tokens = split_command(command_line);
    if (tokens.empty()) {
        return true;
    }
    
    string command = tokens[0];
    vector<string> args(tokens.begin() + 1, tokens.end());
    
    // Handle built-in commands
    if (command == "help" || command == "?") {
        show_help();
        return true;
    } else if (command == "quit" || command == "exit") {
        return false;
    } else if (command == "clear") {
        clear_screen();
        return true;
    } else if (command == "version") {
        print_version();
        return true;
    } else if (command == "status") {
        show_connection_status();
        return true;
    }
    
    // Handle server commands
    return execute_server_command(command, args);
}

/**
 * @brief Execute server command
 */
bool AdminClient::execute_server_command(const string& command, const vector<string>& args) {
    if (!connected) {
        cerr << "Not connected to server" << endl;
        return true;
    }
    
    // Prepare command message
    message_t msg;
    admin_command_t admin_cmd;
    memset(&admin_cmd, 0, sizeof(admin_cmd));
    
    // Map command to message type
    message_type_t msg_type = MSG_INVALID;
    if (command == "list_clients") {
        msg_type = MSG_ADMIN_LIST_CLIENTS;
    } else if (command == "list_jobs") {
        msg_type = MSG_ADMIN_LIST_JOBS;
    } else if (command == "server_stats") {
        msg_type = MSG_ADMIN_SERVER_STATS;
    } else if (command == "disconnect_client") {
        if (args.empty()) {
            cerr << "Usage: disconnect_client <client_id>" << endl;
            return true;
        }
        msg_type = MSG_ADMIN_DISCONNECT_CLIENT;
        admin_cmd.target_id = static_cast<uint32_t>(stoul(args[0]));
    } else if (command == "kill_job") {
        if (args.empty()) {
            cerr << "Usage: kill_job <job_id>" << endl;
            return true;
        }
        msg_type = MSG_ADMIN_KILL_JOB;
        admin_cmd.target_id = static_cast<uint32_t>(stoul(args[0]));
    } else if (command == "shutdown") {
        msg_type = MSG_ADMIN_SERVER_SHUTDOWN;
        cout << "Are you sure you want to shutdown the server? (y/N): ";
        string confirmation;
        getline(cin, confirmation);
        if (confirmation != "y" && confirmation != "Y") {
            cout << "Shutdown cancelled" << endl;
            return true;
        }
    } else {
        cerr << "Unknown command: " << command << endl;
        cerr << "Type 'help' for available commands" << endl;
        return true;
    }
    
    // Send command
    init_message_header(&msg.header, msg_type, sizeof(admin_cmd), generate_correlation_id());
    msg.data = &admin_cmd;
    
    if (!send_message(msg)) {
        cerr << "Failed to send command to server" << endl;
        return true;
    }
    
    // Receive response
    message_t response;
    if (!receive_message(response)) {
        cerr << "Failed to receive response from server" << endl;
        return true;
    }
    
    // Process response
    process_server_response(response);
    
    // Cleanup
    if (response.data) {
        free(response.data);
    }
    
    return true;
}

/**
 * @brief Process server response
 */
void AdminClient::process_server_response(const message_t& response) {
    switch (response.header.message_type) {
        case MSG_ADMIN_LIST_CLIENTS:
            display_client_list(response);
            break;
        case MSG_ADMIN_LIST_JOBS:
            display_job_list(response);
            break;
        case MSG_ADMIN_SERVER_STATS:
            display_server_stats(response);
            break;
        case MSG_ACK:
            cout << "Command executed successfully" << endl;
            break;
        case MSG_ERROR:
            display_error(response);
            break;
        default:
            cout << "Received unknown response type: " << response.header.message_type << endl;
            break;
    }
}

/**
 * @brief Display client list
 */
void AdminClient::display_client_list(const message_t& response) {
    if (!response.data || response.header.data_length == 0) {
        cout << "No clients connected" << endl;
        return;
    }
    
    cout << "\n=== Connected Clients ===" << endl;
    cout << left << setw(8) << "ID" 
         << setw(16) << "IP Address" 
         << setw(8) << "Port"
         << setw(20) << "Connected"
         << setw(12) << "State"
         << setw(8) << "Jobs" << endl;
    cout << string(72, '-') << endl;
    
    // Parse client data (simplified - in real implementation would parse structured data)
    const char* data = static_cast<const char*>(response.data);
    cout << data << endl;
}

/**
 * @brief Display job list
 */
void AdminClient::display_job_list(const message_t& response) {
    if (!response.data || response.header.data_length == 0) {
        cout << "No active jobs" << endl;
        return;
    }
    
    cout << "\n=== Active Jobs ===" << endl;
    cout << left << setw(8) << "Job ID"
         << setw(10) << "Client"
         << setw(12) << "Language"
         << setw(12) << "State"
         << setw(20) << "Started"
         << setw(8) << "PID" << endl;
    cout << string(70, '-') << endl;
    
    // Parse job data
    const char* data = static_cast<const char*>(response.data);
    cout << data << endl;
}

/**
 * @brief Display server statistics
 */
void AdminClient::display_server_stats(const message_t& response) {
    if (!response.data || response.header.data_length < sizeof(server_stats_t)) {
        cout << "Invalid server statistics data" << endl;
        return;
    }
    
    const server_stats_t* stats = static_cast<const server_stats_t*>(response.data);
    
    cout << "\n=== Server Statistics ===" << endl;
    cout << "Server uptime: " << format_duration(time(nullptr) - stats->start_time) << endl;
    cout << "Active clients: " << stats->active_clients << endl;
    cout << "Total clients: " << stats->total_clients << endl;
    cout << "Active jobs: " << stats->active_jobs << endl;
    cout << "Total jobs: " << stats->total_jobs << endl;
    cout << "Completed jobs: " << stats->completed_jobs << endl;
    cout << "Failed jobs: " << stats->failed_jobs << endl;
    cout << "Bytes received: " << format_bytes(stats->total_bytes_received) << endl;
    cout << "Bytes sent: " << format_bytes(stats->total_bytes_sent) << endl;
    cout << "Average response time: " << fixed << setprecision(2) 
         << stats->avg_response_time_ms << " ms" << endl;
    cout << "Memory usage: " << stats->memory_usage_kb << " KB" << endl;
    cout << "CPU usage: " << fixed << setprecision(1) 
         << stats->cpu_usage_percent << "%" << endl;
}

/**
 * @brief Display error message
 */
void AdminClient::display_error(const message_t& response) {
    if (!response.data || response.header.data_length < sizeof(error_payload_t)) {
        cout << "Error: Unknown error occurred" << endl;
        return;
    }
    
    const error_payload_t* error = static_cast<const error_payload_t*>(response.data);
    cout << "Error: " << error->error_message << endl;
    
    if (config.verbose && error->error_context[0] != '\0') {
        cout << "Context: " << error->error_context << endl;
    }
}

/**
 * @brief Show help information
 */
void AdminClient::show_help() {
    cout << "\n=== Admin Client Commands ===" << endl;
    cout << "Server Commands:" << endl;
    cout << "  list_clients              List connected clients" << endl;
    cout << "  list_jobs                 List active jobs" << endl;
    cout << "  server_stats              Show server statistics" << endl;
    cout << "  disconnect_client <id>    Disconnect a client" << endl;
    cout << "  kill_job <id>             Cancel a job" << endl;
    cout << "  shutdown                  Shutdown server" << endl;
    cout << "\nLocal Commands:" << endl;
    cout << "  help, ?                   Show this help" << endl;
    cout << "  status                    Show connection status" << endl;
    cout << "  clear                     Clear screen" << endl;
    cout << "  version                   Show version information" << endl;
    cout << "  quit, exit                Exit admin client" << endl;
    cout << endl;
}

/**
 * @brief Show connection status
 */
void AdminClient::show_connection_status() {
    cout << "\n=== Connection Status ===" << endl;
    cout << "Connected: " << (connected ? "Yes" : "No") << endl;
    if (connected) {
        cout << "Socket path: " << config.socket_path << endl;
        cout << "Connected since: " << format_time(connect_time) << endl;
        cout << "Connection duration: " << format_duration(time(nullptr) - connect_time) << endl;
    }
    cout << endl;
}

/**
 * @brief Clear screen
 */
void AdminClient::clear_screen() {
#ifdef HAVE_NCURSES
    if (config.use_colors) {
        clear();
        refresh();
        return;
    }
#endif
    cout << "\033[2J\033[H" << flush;
}

/**
 * @brief Split command line into tokens
 */
vector<string> AdminClient::split_command(const string& command_line) {
    vector<string> tokens;
    istringstream iss(command_line);
    string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

/**
 * @brief Format time duration
 */
string AdminClient::format_duration(time_t seconds) {
    ostringstream oss;
    
    if (seconds < 60) {
        oss << seconds << "s";
    } else if (seconds < 3600) {
        oss << seconds / 60 << "m " << seconds % 60 << "s";
    } else if (seconds < 86400) {
        oss << seconds / 3600 << "h " << (seconds % 3600) / 60 << "m";
    } else {
        oss << seconds / 86400 << "d " << (seconds % 86400) / 3600 << "h";
    }
    
    return oss.str();
}

/**
 * @brief Format bytes with units
 */
string AdminClient::format_bytes(uint64_t bytes) {
    ostringstream oss;
    oss << fixed << setprecision(1);
    
    if (bytes < 1024) {
        oss << bytes << " B";
    } else if (bytes < 1024 * 1024) {
        oss << static_cast<double>(bytes) / 1024 << " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        oss << static_cast<double>(bytes) / (1024 * 1024) << " MB";
    } else {
        oss << static_cast<double>(bytes) / (1024 * 1024 * 1024) << " GB";
    }
    
    return oss.str();
}

/**
 * @brief Format time
 */
string AdminClient::format_time(time_t timestamp) {
    char buffer[64];
    struct tm* tm_info = localtime(&timestamp);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return string(buffer);
}

/**
 * @brief Run interactive mode
 */
bool AdminClient::run_interactive() {
    cout << "Code Compiler & Executer Admin Client " << ADMIN_CLIENT_VERSION << endl;
    cout << "Type 'help' for available commands, 'quit' to exit" << endl;
    cout << endl;
    
#ifdef HAVE_READLINE
    // Setup readline
    rl_readline_name = "admin_client";
    using_history();
#endif
    
    string command_line;
    while (!g_shutdown_requested) {
        // Check connection
        if (!connected) {
            cerr << "Connection lost. Attempting to reconnect..." << endl;
            if (!connect_to_server()) {
                cerr << "Failed to reconnect. Exiting." << endl;
                break;
            }
        }
        
        // Read command
#ifdef HAVE_READLINE
        char* input = readline("admin> ");
        if (!input) {
            // EOF received
            break;
        }
        
        command_line = input;
        
        // Add to history if not empty
        if (!command_line.empty()) {
            add_history(input);
        }
        
        free(input);
#else
        cout << "admin> ";
        if (!getline(cin, command_line)) {
            // EOF received
            break;
        }
#endif
        
        // Execute command
        if (!execute_command(command_line)) {
            break;
        }
    }
    
    return true;
}

/**
 * @brief Run batch mode
 */
bool AdminClient::run_batch() {
    if (!config.execute_command.empty()) {
        // Execute single command
        if (!execute_command(config.execute_command)) {
            return false;
        }
    } else {
        // Read commands from stdin
        string command_line;
        while (!g_shutdown_requested && getline(cin, command_line)) {
            if (!execute_command(command_line)) {
                break;
            }
        }
    }
    
    return true;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Setup signal handling
    if (!setup_signal_handling()) {
        return 1;
    }
    
    // Parse command line arguments
    if (!parse_arguments(argc, argv, g_admin_client.config)) {
        return 1;
    }
    
    // Load configuration file if specified
    if (!g_admin_client.config.config_file.empty()) {
        // TODO: Implement config file loading
        if (!g_admin_client.config.quiet) {
            cout << "Config file loading not yet implemented" << endl;
        }
    }
    
    // Connect to server
    if (!g_admin_client.connect_to_server()) {
        return 1;
    }
    
    bool success = true;
    
    // Run in appropriate mode
    if (g_admin_client.config.batch_mode) {
        success = g_admin_client.run_batch();
    } else {
        success = g_admin_client.run_interactive();
    }
    
    // Cleanup
    g_admin_client.disconnect_from_server();
    
    return success ? 0 : 1;
}