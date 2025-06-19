/**
 * @file admin_commands.h
 * @brief Admin command definitions for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#ifndef ADMIN_COMMANDS_H
#define ADMIN_COMMANDS_H

#include "admin_client.h"
#include <string>
#include <vector>
#include <map>
#include <functional>

// Forward declarations
class AdminClient;
struct message_t;
struct admin_command_t;
enum message_type_t;

/**
 * @brief Command processor class for handling admin commands
 */
class CommandProcessor {
public:
    explicit CommandProcessor(AdminClient* client);
    ~CommandProcessor() = default;
    
    // Main command handlers
    bool handle_list_clients(const std::vector<std::string>& args);
    bool handle_list_jobs(const std::vector<std::string>& args);
    bool handle_server_stats(const std::vector<std::string>& args);
    bool handle_disconnect_client(const std::vector<std::string>& args);
    bool handle_kill_job(const std::vector<std::string>& args);
    bool handle_server_shutdown(const std::vector<std::string>& args);
    bool handle_server_config(const std::vector<std::string>& args);
    bool handle_monitor(const std::vector<std::string>& args);
    bool handle_logs(const std::vector<std::string>& args);
    
    // Advanced command handlers
    bool handle_bulk_disconnect(const std::vector<std::string>& args);
    bool handle_job_priority(const std::vector<std::string>& args);
    bool handle_client_limit(const std::vector<std::string>& args);
    bool handle_maintenance_mode(const std::vector<std::string>& args);
    bool handle_backup_create(const std::vector<std::string>& args);
    bool handle_backup_restore(const std::vector<std::string>& args);
    
    // Monitoring and reporting
    bool handle_real_time_monitor();
    bool handle_generate_report(const std::vector<std::string>& args);
    bool handle_export_data(const std::vector<std::string>& args);
    bool handle_system_health(const std::vector<std::string>& args);
    bool handle_performance_analysis(const std::vector<std::string>& args);
    
    // Utility commands
    bool handle_ping_server(const std::vector<std::string>& args);
    bool handle_test_connection(const std::vector<std::string>& args);
    bool handle_debug_info(const std::vector<std::string>& args);
    
private:
    AdminClient* admin_client;
    
    // Helper methods
    bool send_command_and_wait(message_type_t type, const admin_command_t& cmd);
    void display_table(const std::vector<std::vector<std::string>>& data, 
                      const std::vector<std::string>& headers);
    void display_progress_bar(int current, int total, const std::string& label);
    std::string format_current_time();
    
    // Data processing helpers
    std::vector<std::vector<std::string>> parse_client_data(const void* data, size_t size);
    std::vector<std::vector<std::string>> parse_job_data(const void* data, size_t size);
    void format_server_stats_display(const void* data, size_t size);
    
    // Monitoring helpers
    void display_monitoring_header();
    void display_client_summary(const std::vector<std::vector<std::string>>& clients);
    void display_job_summary(const std::vector<std::vector<std::string>>& jobs);
    void display_performance_metrics(const void* stats_data);
    
    // Configuration helpers
    bool validate_config_key(const std::string& key);
    bool validate_config_value(const std::string& key, const std::string& value);
    void show_config_help();
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
    
    // Status indicators
    static std::string get_status_indicator(const std::string& status);
    static std::string get_priority_indicator(int priority);
    static std::string get_health_indicator(float health_percentage);
    
    // Cursor control
    static void move_cursor(int row, int col);
    static void clear_line();
    static void clear_screen();
    static void hide_cursor();
    static void show_cursor();
    static void save_cursor_position();
    static void restore_cursor_position();
    
    // Input handling
    static char get_single_char();
    static std::string get_password_input();
    static bool confirm_yes_no(const std::string& prompt);
    static int get_menu_choice(const std::vector<std::string>& options, const std::string& prompt);
    
    // Box drawing
    static void draw_box(int x, int y, int width, int height, const std::string& title = "");
    static void draw_horizontal_line(int length, char character = '-');
    static void draw_vertical_line(int length, char character = '|');
};

/**
 * @brief Statistics and performance tracking
 */
class PerformanceTracker {
public:
    PerformanceTracker();
    
    // Command timing
    void start_command_timer(const std::string& command);
    void end_command_timer(const std::string& command, bool success);
    
    // Response time tracking
    void record_response_time(double milliseconds);
    void record_network_stats(size_t bytes_sent, size_t bytes_received);
    
    // Error tracking
    void record_error(const std::string& error_type, const std::string& details);
    void record_timeout();
    void record_connection_failure();
    
    // Statistics retrieval
    double get_average_response_time() const;
    double get_success_rate() const;
    size_t get_total_commands() const;
    size_t get_total_errors() const;
    
    // Reporting
    void print_session_summary() const;
    void print_performance_report() const;
    void export_statistics(const std::string& filename) const;
    
    // Reset statistics
    void reset_all_stats();
    void reset_session_stats();
    
private:
    struct CommandStats {
        size_t count = 0;
        size_t success_count = 0;
        double total_time = 0.0;
        double min_time = std::numeric_limits<double>::max();
        double max_time = 0.0;
    };
    
    std::map<std::string, CommandStats> command_stats;
    
    // Session statistics
    time_t session_start;
    size_t total_commands;
    size_t successful_commands;
    size_t failed_commands;
    size_t timeouts;
    size_t connection_failures;
    
    // Network statistics
    size_t total_bytes_sent;
    size_t total_bytes_received;
    
    // Response time statistics
    std::vector<double> response_times;
    double total_response_time;
    double min_response_time;
    double max_response_time;
    
    // Error statistics
    std::map<std::string, size_t> error_counts;
    std::vector<std::pair<time_t, std::string>> error_log;
    
    // Helper methods
    double calculate_average(const std::vector<double>& values) const;
    double calculate_percentile(const std::vector<double>& values, double percentile) const;
};

/**
 * @brief Command completion and suggestion system
 */
class CommandCompletion {
public:
    CommandCompletion();
    
    // Command completion
    std::vector<std::string> complete_command(const std::string& partial_command);
    std::vector<std::string> complete_arguments(const std::string& command, 
                                               const std::vector<std::string>& partial_args);
    
    // Suggestion system
    std::vector<std::string> suggest_similar_commands(const std::string& invalid_command);
    std::string get_command_help(const std::string& command);
    
    // Command registration
    void register_command(const std::string& command, const std::string& description,
                         const std::vector<std::string>& arguments = {});
    void register_alias(const std::string& alias, const std::string& command);
    
    // Help system
    void show_all_commands() const;
    void show_command_help(const std::string& command) const;
    void show_quick_help() const;
    
private:
    struct CommandInfo {
        std::string description;
        std::vector<std::string> arguments;
        std::vector<std::string> examples;
        std::string category;
    };
    
    std::map<std::string, CommandInfo> commands;
    std::map<std::string, std::string> aliases;
    std::map<std::string, std::vector<std::string>> categories;
    
    // Helper methods
    int calculate_edit_distance(const std::string& s1, const std::string& s2);
    std::vector<std::string> tokenize_command_line(const std::string& line);
    bool matches_prefix(const std::string& text, const std::string& prefix);
};

/**
 * @brief Configuration validator and manager
 */
class ConfigValidator {
public:
    // Validation methods
    static bool validate_timeout_value(const std::string& value);
    static bool validate_port_number(const std::string& value);
    static bool validate_file_path(const std::string& value);
    static bool validate_boolean_value(const std::string& value);
    static bool validate_log_level(const std::string& value);
    static bool validate_memory_size(const std::string& value);
    
    // Configuration constraints
    static bool check_config_constraints(const std::string& key, const std::string& value);
    static std::vector<std::string> get_valid_values(const std::string& key);
    static std::string get_config_description(const std::string& key);
    
    // Configuration help
    static void show_config_reference();
    static void show_config_examples();
    
private:
    static std::map<std::string, std::function<bool(const std::string&)>> validators;
    static std::map<std::string, std::vector<std::string>> valid_values;
    static std::map<std::string, std::string> descriptions;
};

/**
 * @brief Data export and import utilities
 */
class DataExporter {
public:
    // Export formats
    enum class ExportFormat {
        JSON,
        CSV,
        XML,
        TEXT,
        YAML
    };
    
    // Export methods
    static bool export_client_list(const std::vector<std::vector<std::string>>& data,
                                  const std::string& filename, ExportFormat format);
    static bool export_job_list(const std::vector<std::vector<std::string>>& data,
                               const std::string& filename, ExportFormat format);
    static bool export_server_stats(const void* stats_data, size_t data_size,
                                   const std::string& filename, ExportFormat format);
    
    // Import methods
    static bool import_configuration(const std::string& filename, 
                                   std::map<std::string, std::string>& config);
    static bool import_user_list(const std::string& filename,
                                std::vector<std::string>& users);
    
    // Utility methods
    static ExportFormat parse_format_string(const std::string& format);
    static std::string get_format_extension(ExportFormat format);
    static bool validate_export_path(const std::string& path);
    
private:
    // Format-specific exporters
    static bool export_as_json(const std::vector<std::vector<std::string>>& data,
                              const std::vector<std::string>& headers,
                              const std::string& filename);
    static bool export_as_csv(const std::vector<std::vector<std::string>>& data,
                             const std::vector<std::string>& headers,
                             const std::string& filename);
    static bool export_as_xml(const std::vector<std::vector<std::string>>& data,
                             const std::vector<std::string>& headers,
                             const std::string& filename);
};

// Utility functions
namespace AdminUtils {
    // String utilities
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    bool starts_with(const std::string& str, const std::string& prefix);
    bool ends_with(const std::string& str, const std::string& suffix);
    std::string to_lower(const std::string& str);
    std::string to_upper(const std::string& str);
    
    // Numeric utilities
    bool string_to_uint32(const std::string& str, uint32_t& value);
    bool string_to_int(const std::string& str, int& value);
    bool string_to_double(const std::string& str, double& value);
    
    // Time utilities
    std::string format_duration(time_t seconds);
    std::string format_timestamp(time_t timestamp);
    std::string format_relative_time(time_t timestamp);
    time_t parse_time_string(const std::string& time_str);
    
    // Size utilities
    std::string format_bytes(uint64_t bytes);
    std::string format_rate(uint64_t bytes, time_t duration);
    uint64_t parse_size_string(const std::string& size_str);
    
    // Validation utilities
    bool is_valid_client_id(uint32_t id);
    bool is_valid_job_id(uint32_t id);
    bool is_valid_ip_address(const std::string& ip);
    bool is_valid_hostname(const std::string& hostname);
    
    // File utilities
    bool file_exists(const std::string& path);
    bool is_writable_directory(const std::string& path);
    std::string get_file_extension(const std::string& filename);
    std::string generate_unique_filename(const std::string& base_name);
    
    // System utilities
    std::string get_current_user();
    std::string get_hostname();
    std::pair<int, int> get_terminal_dimensions();
    bool is_running_in_terminal();
}

// Command function type
using CommandFunction = std::function<bool(const std::vector<std::string>&)>;

// Command registry for dynamic command handling
class CommandRegistry {
public:
    static CommandRegistry& instance();
    
    // Command registration
    void register_command(const std::string& name, CommandFunction func,
                         const std::string& description, const std::string& usage = "");
    void register_alias(const std::string& alias, const std::string& command);
    
    // Command execution
    bool execute_command(const std::string& command, const std::vector<std::string>& args);
    bool has_command(const std::string& command) const;
    
    // Command information
    std::vector<std::string> get_all_commands() const;
    std::vector<std::string> get_matching_commands(const std::string& prefix) const;
    std::string get_command_description(const std::string& command) const;
    std::string get_command_usage(const std::string& command) const;
    
    // Help system
    void show_help() const;
    void show_command_help(const std::string& command) const;
    
private:
    struct CommandEntry {
        CommandFunction function;
        std::string description;
        std::string usage;
    };
    
    std::map<std::string, CommandEntry> commands;
    std::map<std::string, std::string> aliases;
    
    CommandRegistry() = default;
};

// Macro for easy command registration
#define REGISTER_ADMIN_COMMAND(name, func, desc, usage) \
    CommandRegistry::instance().register_command(name, \
        [this](const std::vector<std::string>& args) { return func(args); }, \
        desc, usage)

// Global utility functions
std::string escape_shell_argument(const std::string& arg);
std::string unescape_shell_argument(const std::string& arg);
std::vector<std::string> parse_command_line(const std::string& line);
bool is_command_safe(const std::string& command);

// Error handling utilities
class AdminError {
public:
    enum class Type {
        NONE,
        CONNECTION_FAILED,
        INVALID_COMMAND,
        INVALID_ARGUMENT,
        SERVER_ERROR,
        PERMISSION_DENIED,
        TIMEOUT,
        NETWORK_ERROR,
        FILE_ERROR,
        CONFIGURATION_ERROR
    };
    
    AdminError(Type type = Type::NONE, const std::string& message = "");
    
    Type get_type() const { return type_; }
    const std::string& get_message() const { return message_; }
    const std::string& get_details() const { return details_; }
    
    void set_details(const std::string& details) { details_ = details; }
    
    operator bool() const { return type_ != Type::NONE; }
    
    std::string to_string() const;
    
    static AdminError connection_failed(const std::string& details = "");
    static AdminError invalid_command(const std::string& command);
    static AdminError invalid_argument(const std::string& argument);
    static AdminError server_error(const std::string& message);
    static AdminError permission_denied(const std::string& operation = "");
    static AdminError timeout(const std::string& operation = "");
    static AdminError network_error(const std::string& details = "");
    
private:
    Type type_;
    std::string message_;
    std::string details_;
};

// Thread-safe logging for admin client
class AdminLogger {
public:
    enum class Level {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        ERROR = 4,
        CRITICAL = 5
    };
    
    static AdminLogger& instance();
    
    // Configuration
    void set_level(Level level);
    void set_output_file(const std::string& filename);
    void enable_console_output(bool enable);
    void enable_timestamps(bool enable);
    void enable_colors(bool enable);
    
    // Logging methods
    void log(Level level, const std::string& message);
    void trace(const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void critical(const std::string& message);
    
    // Formatted logging
    template<typename... Args>
    void log_formatted(Level level, const std::string& format, Args&&... args);
    
    // Session logging
    void start_session();
    void end_session();
    void log_command(const std::string& command, bool success);
    
private:
    Level current_level_;
    std::string output_file_;
    bool console_output_;
    bool timestamps_;
    bool colors_;
    std::mutex log_mutex_;
    std::ofstream file_stream_;
    
    AdminLogger();
    ~AdminLogger();
    
    std::string level_to_string(Level level) const;
    std::string get_timestamp() const;
    std::string colorize_level(Level level, const std::string& text) const;
};

// Macros for convenient logging
#define ADMIN_LOG_TRACE(msg) AdminLogger::instance().trace(msg)
#define ADMIN_LOG_DEBUG(msg) AdminLogger::instance().debug(msg)
#define ADMIN_LOG_INFO(msg) AdminLogger::instance().info(msg)
#define ADMIN_LOG_WARNING(msg) AdminLogger::instance().warning(msg)
#define ADMIN_LOG_ERROR(msg) AdminLogger::instance().error(msg)
#define ADMIN_LOG_CRITICAL(msg) AdminLogger::instance().critical(msg)

// Session management
class AdminSession {
public:
    AdminSession();
    ~AdminSession();
    
    // Session info
    time_t get_start_time() const { return start_time_; }
    time_t get_duration() const { return time(nullptr) - start_time_; }
    const std::string& get_session_id() const { return session_id_; }
    
    // Statistics
    void increment_command_count() { ++command_count_; }
    void increment_error_count() { ++error_count_; }
    void add_response_time(double time_ms) { 
        total_response_time_ += time_ms;
        ++response_count_;
    }
    
    size_t get_command_count() const { return command_count_; }
    size_t get_error_count() const { return error_count_; }
    double get_average_response_time() const {
        return response_count_ > 0 ? total_response_time_ / response_count_ : 0.0;
    }
    
    // Session state
    void set_connected(bool connected) { connected_ = connected; }
    bool is_connected() const { return connected_; }
    
    // Export session data
    bool export_session_log(const std::string& filename) const;
    
private:
    std::string session_id_;
    time_t start_time_;
    bool connected_;
    
    // Statistics
    size_t command_count_;
    size_t error_count_;
    size_t response_count_;
    double total_response_time_;
    
    // Session log
    std::vector<std::pair<time_t, std::string>> command_history_;
    
    std::string generate_session_id() const;
};

// Keyboard shortcuts and hotkeys
class KeyboardHandler {
public:
    using KeyCallback = std::function<bool()>;
    
    static KeyboardHandler& instance();
    
    // Hotkey registration
    void register_hotkey(const std::string& key_combination, KeyCallback callback);
    void unregister_hotkey(const std::string& key_combination);
    
    // Input handling
    void start_input_handling();
    void stop_input_handling();
    bool handle_key_press(int key);
    
    // Default hotkeys
    void setup_default_hotkeys();
    
private:
    std::map<std::string, KeyCallback> hotkeys_;
    bool handling_input_;
    std::thread input_thread_;
    
    KeyboardHandler() = default;
    ~KeyboardHandler();
    
    void input_handler_thread();
    std::string key_to_string(int key);
};

#endif /* ADMIN_COMMANDS_H */