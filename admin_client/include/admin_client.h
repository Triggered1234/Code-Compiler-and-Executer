/**
 * @file admin_client.h
 * @brief Administration client definitions for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#ifndef ADMIN_CLIENT_H
#define ADMIN_CLIENT_H

#include "protocol.h"
#include <string>
#include <vector>
#include <ctime>

// Version information
#define ADMIN_CLIENT_VERSION "1.0.0"

// Default configuration
#define DEFAULT_ADMIN_SOCKET "/tmp/code_server_admin.sock"
#define DEFAULT_TIMEOUT 30
#define DEFAULT_CONFIG_FILE "~/.config/code_server/admin.conf"

/**
 * @brief Admin client configuration
 */
struct AdminClientConfig {
    std::string socket_path = DEFAULT_ADMIN_SOCKET;
    std::string config_file;
    std::string execute_command;
    int timeout = DEFAULT_TIMEOUT;
    bool batch_mode = false;
    bool verbose = false;
    bool quiet = false;
    bool use_colors = true;
    bool auto_reconnect = true;
    int refresh_interval = 5;
    
    AdminClientConfig() = default;
};

/**
 * @brief Server statistics structure (matches server-side)
 */
struct server_stats_t {
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
};

/**
 * @brief Error payload structure (matches server-side)
 */
struct error_payload_t {
    uint32_t error_code;
    uint32_t error_line;
    char error_message[4096];
    char error_context[256];
};

/**
 * @brief Admin command structure (matches server-side)
 */
struct admin_command_t {
    uint16_t command_type;
    uint16_t flags;
    uint32_t target_id;
    char command_data[512];
};

/**
 * @brief Message structure (matches protocol)
 */
struct message_t {
    message_header_t header;
    void* data;
    
    message_t() : data(nullptr) {}
    ~message_t() {
        if (data) {
            free(data);
            data = nullptr;
        }
    }
};

/**
 * @brief Client information for display
 */
struct ClientInfo {
    uint32_t client_id;
    std::string ip_address;
    int port;
    std::string state;
    time_t connect_time;
    time_t last_activity;
    uint32_t active_jobs;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    std::string client_name;
    std::string platform;
};

/**
 * @brief Job information for display
 */
struct JobInfo {
    uint32_t job_id;
    uint32_t client_id;
    std::string language;
    std::string state;
    time_t submit_time;
    time_t start_time;
    pid_t process_id;
    std::string source_file;
    int priority;
    double estimated_time;
};

/**
 * @brief Command history entry
 */
struct CommandHistoryEntry {
    std::string command;
    time_t timestamp;
    bool success;
    std::string result;
};

/**
 * @brief Main admin client class
 */
class AdminClient {
public:
    AdminClient();
    ~AdminClient();
    
    // Configuration
    AdminClientConfig config;
    
    // Connection management
    bool connect_to_server();
    void disconnect_from_server();
    bool is_connected() const { return connected; }
    
    // Command execution
    bool execute_command(const std::string& command_line);
    bool execute_server_command(const std::string& command, const std::vector<std::string>& args);
    
    // Running modes
    bool run_interactive();
    bool run_batch();
    
    // Message handling
    bool send_message(const message_t& msg);
    bool receive_message(message_t& msg);
    
    // Display functions
    void display_client_list(const message_t& response);
    void display_job_list(const message_t& response);
    void display_server_stats(const message_t& response);
    void display_error(const message_t& response);
    
    // Utility functions
    void show_help();
    void show_connection_status();
    void clear_screen();
    
    // Command history
    void add_to_history(const std::string& command, bool success, const std::string& result);
    void show_history();
    void clear_history();
    
    // Configuration management
    bool load_config(const std::string& filename);
    bool save_config(const std::string& filename);
    
private:
    // Connection state
    int socket_fd;
    bool connected;
    time_t connect_time;
    time_t last_activity;
    
    // Command history
    std::vector<CommandHistoryEntry> command_history;
    size_t max_history_size;
    
    // Statistics
    uint32_t commands_sent;
    uint32_t responses_received;
    uint32_t errors_received;
    
    // Internal methods
    bool send_admin_connect();
    bool send_admin_disconnect();
    void process_server_response(const message_t& response);
    std::vector<std::string> split_command(const std::string& command_line);
    
    // Formatting helpers
    std::string format_duration(time_t seconds);
    std::string format_bytes(uint64_t bytes);
    std::string format_time(time_t timestamp);
    std::string format_percentage(float value);
    std::string format_client_state(const std::string& state);
    std::string format_job_state(const std::string& state);
    
    // Color support
    bool supports_colors() const;
    std::string colorize(const std::string& text, const std::string& color);
    
    // Input handling
    std::string read_command_line();
    std::string read_password();
    bool confirm_action(const std::string& prompt);
    
    // Validation
    bool validate_client_id(const std::string& id_str, uint32_t& id);
    bool validate_job_id(const std::string& id_str, uint32_t& id);
    
    // Auto-completion support
    std::vector<std::string> get_command_completions(const std::string& partial);
    std::vector<std::string> get_argument_completions(const std::string& command, const std::string& partial);
};

/**
 * @brief Command processor class for handling complex commands
 */
class CommandProcessor {
public:
    CommandProcessor(AdminClient* client);
    
    // Command handlers
    bool handle_list_clients(const std::vector<std::string>& args);
    bool handle_list_jobs(const std::vector<std::string>& args);
    bool handle_server_stats(const std::vector<std::string>& args);
    bool handle_disconnect_client(const std::vector<std::string>& args);
    bool handle_kill_job(const std::vector<std::string>& args);
    bool handle_server_shutdown(const std::vector<std::string>& args);
    bool handle_server_config(const std::vector<std::string>& args);
    bool handle_monitor(const std::vector<std::string>& args);
    bool handle_logs(const std::vector<std::string>& args);
    
    // Advanced commands
    bool handle_bulk_disconnect(const std::vector<std::string>& args);
    bool handle_job_priority(const std::vector<std::string>& args);
    bool handle_client_limit(const std::vector<std::string>& args);
    bool handle_maintenance_mode(const std::vector<std::string>& args);
    
    // Monitoring and reporting
    bool handle_real_time_monitor();
    bool handle_generate_report(const std::vector<std::string>& args);
    bool handle_export_data(const std::vector<std::string>& args);
    
private:
    AdminClient* admin_client;
    
    // Helper methods
    bool send_command_and_wait(message_type_t type, const admin_command_t& cmd);
    void display_table(const std::vector<std::vector<std::string>>& data, 
                      const std::vector<std::string>& headers);
    void display_progress_bar(int current, int total, const std::string& label);
};

/**
 * @brief Configuration manager class
 */
class ConfigManager {
public:
    static bool load_config_file(const std::string& filename, AdminClientConfig& config);
    static bool save_config_file(const std::string& filename, const AdminClientConfig& config);
    static void set_default_config(AdminClientConfig& config);
    static bool validate_config(const AdminClientConfig& config);
    
private:
    static bool parse_config_line(const std::string& line, std::string& key, std::string& value);
    static void apply_config_value(AdminClientConfig& config, const std::string& key, const std::string& value);
};

/**
 * @brief Terminal utilities for enhanced display
 */
class TerminalUtils {
public:
    // Color constants
    static const std::string COLOR_RED;
    static const std::string COLOR_GREEN;
    static const std::string COLOR_YELLOW;
    static const std::string COLOR_BLUE;
    static const std::string COLOR_MAGENTA;
    static const std::string COLOR_CYAN;
    static const std::string COLOR_WHITE;
    static const std::string COLOR_RESET;
    
    // Style constants
    static const std::string STYLE_BOLD;
    static const std::string STYLE_DIM;
    static const std::string STYLE_UNDERLINE;
    static const std::string STYLE_RESET;
    
    // Terminal capabilities
    static bool supports_colors();
    static bool supports_unicode();
    static std::pair<int, int> get_terminal_size();
    
    // Text formatting
    static std::string colorize(const std::string& text, const std::string& color);
    static std::string stylize(const std::string& text, const std::string& style);
    static std::string center_text(const std::string& text, int width);
    static std::string truncate_text(const std::string& text, int max_width);
    static std::string pad_text(const std::string& text, int width, bool align_right = false);
    
    // Special characters and symbols
    static std::string get_check_mark();
    static std::string get_cross_mark();
    static std::string get_warning_symbol();
    static std::string get_info_symbol();
    static std::string get_progress_bar(int percentage, int width = 20);
    
    // Cursor control
    static void move_cursor(int row, int col);
    static void clear_line();
    static void clear_screen();
    static void hide_cursor();
    static void show_cursor();
    
    // Input handling
    static char get_single_char();
    static std::string get_password_input();
    static bool confirm_yes_no(const std::string& prompt);
};

/**
 * @brief Statistics tracker for monitoring admin client performance
 */
class StatsTracker {
public:
    StatsTracker();
    
    // Command statistics
    void record_command_sent(const std::string& command);
    void record_response_received(bool success, double response_time);
    void record_error(const std::string& error_type);
    
    // Connection statistics
    void record_connection_attempt();
    void record_connection_success();
    void record_connection_failure();
    void record_disconnection();
    
    // Display statistics
    void show_session_stats();
    void show_performance_stats();
    void reset_stats();
    
    // Export statistics
    bool export_stats_to_file(const std::string& filename);
    
private:
    struct CommandStats {
        uint32_t count;
        double total_time;
        double min_time;
        double max_time;
        uint32_t errors;
    };
    
    std::map<std::string, CommandStats> command_stats;
    
    uint32_t total_commands;
    uint32_t successful_responses;
    uint32_t error_responses;
    uint32_t connection_attempts;
    uint32_t successful_connections;
    uint32_t failed_connections;
    
    time_t session_start;
    time_t last_activity;
    
    double total_response_time;
    double min_response_time;
    double max_response_time;
};

// Global functions
void signal_handler(int sig);
bool setup_signal_handling();
void print_usage(const char* program_name);
void print_version();
bool parse_arguments(int argc, char* argv[], AdminClientConfig& config);

// Utility functions for protocol handling
uint32_t generate_correlation_id();
void init_message_header(message_header_t* header, message_type_t type, 
                        uint32_t data_length, uint32_t correlation_id);
int validate_message_header(const message_header_t* header);
void header_to_network(message_header_t* header);
void header_from_network(message_header_t* header);

// Global variables
extern AdminClient g_admin_client;
extern volatile bool g_shutdown_requested;

// Inline utility functions
inline std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

inline std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

inline bool string_to_uint32(const std::string& str, uint32_t& value) {
    try {
        unsigned long result = std::stoul(str);
        if (result > UINT32_MAX) return false;
        value = static_cast<uint32_t>(result);
        return true;
    } catch (...) {
        return false;
    }
}

inline std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

inline std::string to_upper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

#endif /* ADMIN_CLIENT_H */