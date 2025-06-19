/**
 * @file admin_commands.cpp
 * @brief Admin command implementations for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#include "admin_commands.h"
#include "admin_client.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <fstream>

using namespace std;

/**
 * @brief Constructor
 */
CommandProcessor::CommandProcessor(AdminClient* client) : admin_client(client) {
}

/**
 * @brief Handle list_clients command
 */
bool CommandProcessor::handle_list_clients(const vector<string>& args) {
    bool detailed = false;
    string filter;
    
    // Parse arguments
    for (const auto& arg : args) {
        if (arg == "-d" || arg == "--detailed") {
            detailed = true;
        } else if (arg.substr(0, 9) == "--filter=") {
            filter = arg.substr(9);
        } else if (arg == "-h" || arg == "--help") {
            cout << "Usage: list_clients [-d|--detailed] [--filter=pattern]\n";
            cout << "  -d, --detailed     Show detailed client information\n";
            cout << "  --filter=pattern   Filter clients by IP or name\n";
            return true;
        }
    }
    
    // Send command to server
    admin_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = 1; // LIST_CLIENTS
    cmd.flags = detailed ? 1 : 0;
    strncpy(cmd.command_data, filter.c_str(), sizeof(cmd.command_data) - 1);
    
    return send_command_and_wait(MSG_ADMIN_LIST_CLIENTS, cmd);
}

/**
 * @brief Handle list_jobs command
 */
bool CommandProcessor::handle_list_jobs(const vector<string>& args) {
    bool active_only = true;
    bool show_completed = false;
    uint32_t client_id = 0;
    
    // Parse arguments
    for (size_t i = 0; i < args.size(); ++i) {
        const string& arg = args[i];
        
        if (arg == "-a" || arg == "--all") {
            active_only = false;
        } else if (arg == "-c" || arg == "--completed") {
            show_completed = true;
            active_only = false;
        } else if (arg == "--client" && i + 1 < args.size()) {
            if (!string_to_uint32(args[++i], client_id)) {
                cerr << "Error: Invalid client ID: " << args[i] << endl;
                return true;
            }
        } else if (arg == "-h" || arg == "--help") {
            cout << "Usage: list_jobs [-a|--all] [-c|--completed] [--client <id>]\n";
            cout << "  -a, --all         Show all jobs (active and completed)\n";
            cout << "  -c, --completed   Show only completed jobs\n";
            cout << "  --client <id>     Show jobs for specific client\n";
            return true;
        }
    }
    
    admin_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = 2; // LIST_JOBS
    cmd.flags = (active_only ? 1 : 0) | (show_completed ? 2 : 0);
    cmd.target_id = client_id;
    
    return send_command_and_wait(MSG_ADMIN_LIST_JOBS, cmd);
}

/**
 * @brief Handle server_stats command
 */
bool CommandProcessor::handle_server_stats(const vector<string>& args) {
    bool detailed = false;
    bool json_format = false;
    
    // Parse arguments
    for (const auto& arg : args) {
        if (arg == "-d" || arg == "--detailed") {
            detailed = true;
        } else if (arg == "-j" || arg == "--json") {
            json_format = true;
        } else if (arg == "-h" || arg == "--help") {
            cout << "Usage: server_stats [-d|--detailed] [-j|--json]\n";
            cout << "  -d, --detailed     Show detailed statistics\n";
            cout << "  -j, --json         Output in JSON format\n";
            return true;
        }
    }
    
    admin_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = 3; // SERVER_STATS
    cmd.flags = (detailed ? 1 : 0) | (json_format ? 2 : 0);
    
    return send_command_and_wait(MSG_ADMIN_SERVER_STATS, cmd);
}

/**
 * @brief Handle disconnect_client command
 */
bool CommandProcessor::handle_disconnect_client(const vector<string>& args) {
    if (args.empty()) {
        cerr << "Usage: disconnect_client <client_id> [--force]\n";
        cerr << "  <client_id>    ID of the client to disconnect\n";
        cerr << "  --force        Force disconnection without graceful shutdown\n";
        return true;
    }
    
    uint32_t client_id;
    if (!string_to_uint32(args[0], client_id)) {
        cerr << "Error: Invalid client ID: " << args[0] << endl;
        return true;
    }
    
    bool force = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--force") {
            force = true;
        } else if (args[i] == "-h" || args[i] == "--help") {
            cout << "Usage: disconnect_client <client_id> [--force]\n";
            cout << "  <client_id>    ID of the client to disconnect\n";
            cout << "  --force        Force disconnection without graceful shutdown\n";
            return true;
        }
    }
    
    // Confirm action
    if (!admin_client->config.batch_mode) {
        cout << "Are you sure you want to disconnect client " << client_id;
        if (force) cout << " (forced)";
        cout << "? (y/N): ";
        
        string confirmation;
        getline(cin, confirmation);
        if (confirmation != "y" && confirmation != "Y") {
            cout << "Operation cancelled" << endl;
            return true;
        }
    }
    
    admin_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = 4; // DISCONNECT_CLIENT
    cmd.target_id = client_id;
    cmd.flags = force ? 1 : 0;
    
    return send_command_and_wait(MSG_ADMIN_DISCONNECT_CLIENT, cmd);
}

/**
 * @brief Handle kill_job command
 */
bool CommandProcessor::handle_kill_job(const vector<string>& args) {
    if (args.empty()) {
        cerr << "Usage: kill_job <job_id> [--force]\n";
        cerr << "  <job_id>    ID of the job to cancel\n";
        cerr << "  --force     Force kill the process\n";
        return true;
    }
    
    uint32_t job_id;
    if (!string_to_uint32(args[0], job_id)) {
        cerr << "Error: Invalid job ID: " << args[0] << endl;
        return true;
    }
    
    bool force = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--force") {
            force = true;
        } else if (args[i] == "-h" || args[i] == "--help") {
            cout << "Usage: kill_job <job_id> [--force]\n";
            cout << "  <job_id>    ID of the job to cancel\n";
            cout << "  --force     Force kill the process\n";
            return true;
        }
    }
    
    // Confirm action
    if (!admin_client->config.batch_mode) {
        cout << "Are you sure you want to kill job " << job_id;
        if (force) cout << " (forced)";
        cout << "? (y/N): ";
        
        string confirmation;
        getline(cin, confirmation);
        if (confirmation != "y" && confirmation != "Y") {
            cout << "Operation cancelled" << endl;
            return true;
        }
    }
    
    admin_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = 5; // KILL_JOB
    cmd.target_id = job_id;
    cmd.flags = force ? 1 : 0;
    
    return send_command_and_wait(MSG_ADMIN_KILL_JOB, cmd);
}

/**
 * @brief Handle server_shutdown command
 */
bool CommandProcessor::handle_server_shutdown(const vector<string>& args) {
    bool graceful = true;
    int delay = 0;
    
    // Parse arguments
    for (size_t i = 0; i < args.size(); ++i) {
        const string& arg = args[i];
        
        if (arg == "--force") {
            graceful = false;
        } else if (arg == "--delay" && i + 1 < args.size()) {
            delay = atoi(args[++i].c_str());
            if (delay < 0) {
                cerr << "Error: Invalid delay value: " << args[i] << endl;
                return true;
            }
        } else if (arg == "-h" || arg == "--help") {
            cout << "Usage: shutdown [--force] [--delay <seconds>]\n";
            cout << "  --force         Force immediate shutdown\n";
            cout << "  --delay <sec>   Delay shutdown by specified seconds\n";
            return true;
        }
    }
    
    // Multiple confirmations for shutdown
    if (!admin_client->config.batch_mode) {
        cout << "\n" << TerminalUtils::colorize("WARNING", TerminalUtils::COLOR_RED) 
             << ": This will shutdown the entire server!\n";
        cout << "All connected clients will be disconnected.\n";
        cout << "All running jobs will be terminated.\n\n";
        
        cout << "Type 'SHUTDOWN' to confirm: ";
        string confirmation;
        getline(cin, confirmation);
        if (confirmation != "SHUTDOWN") {
            cout << "Shutdown cancelled" << endl;
            return true;
        }
        
        cout << "Are you absolutely sure? (y/N): ";
        getline(cin, confirmation);
        if (confirmation != "y" && confirmation != "Y") {
            cout << "Shutdown cancelled" << endl;
            return true;
        }
    }
    
    if (delay > 0) {
        cout << "Server shutdown scheduled in " << delay << " seconds..." << endl;
        // In a real implementation, we might want to show a countdown
    }
    
    admin_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = 6; // SERVER_SHUTDOWN
    cmd.flags = (graceful ? 0 : 1) | ((delay > 0) ? 2 : 0);
    cmd.target_id = delay;
    
    return send_command_and_wait(MSG_ADMIN_SERVER_SHUTDOWN, cmd);
}

/**
 * @brief Handle server configuration commands
 */
bool CommandProcessor::handle_server_config(const vector<string>& args) {
    if (args.empty()) {
        cout << "Usage: config <get|set|list> [key] [value]\n";
        cout << "  get <key>       Get configuration value\n";
        cout << "  set <key> <val> Set configuration value\n";
        cout << "  list            List all configuration keys\n";
        return true;
    }
    
    string action = args[0];
    
    if (action == "list") {
        admin_command_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.command_type = 7; // CONFIG_LIST
        return send_command_and_wait(MSG_ADMIN_CONFIG_GET, cmd);
        
    } else if (action == "get") {
        if (args.size() < 2) {
            cerr << "Usage: config get <key>" << endl;
            return true;
        }
        
        admin_command_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.command_type = 8; // CONFIG_GET
        strncpy(cmd.command_data, args[1].c_str(), sizeof(cmd.command_data) - 1);
        return send_command_and_wait(MSG_ADMIN_CONFIG_GET, cmd);
        
    } else if (action == "set") {
        if (args.size() < 3) {
            cerr << "Usage: config set <key> <value>" << endl;
            return true;
        }
        
        // Confirm configuration change
        if (!admin_client->config.batch_mode) {
            cout << "Set '" << args[1] << "' to '" << args[2] << "'? (y/N): ";
            string confirmation;
            getline(cin, confirmation);
            if (confirmation != "y" && confirmation != "Y") {
                cout << "Configuration change cancelled" << endl;
                return true;
            }
        }
        
        admin_command_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.command_type = 9; // CONFIG_SET
        
        // Pack key=value into command_data
        string config_data = args[1] + "=" + args[2];
        strncpy(cmd.command_data, config_data.c_str(), sizeof(cmd.command_data) - 1);
        
        return send_command_and_wait(MSG_ADMIN_CONFIG_SET, cmd);
        
    } else {
        cerr << "Unknown config action: " << action << endl;
        cerr << "Valid actions: get, set, list" << endl;
        return true;
    }
}

/**
 * @brief Handle real-time monitoring
 */
bool CommandProcessor::handle_monitor(const vector<string>& args) {
    int refresh_interval = 5;
    bool show_jobs = true;
    bool show_clients = true;
    bool show_stats = true;
    
    // Parse arguments
    for (size_t i = 0; i < args.size(); ++i) {
        const string& arg = args[i];
        
        if (arg == "-i" || arg == "--interval") {
            if (i + 1 < args.size()) {
                refresh_interval = atoi(args[++i].c_str());
                if (refresh_interval < 1) refresh_interval = 1;
            }
        } else if (arg == "--no-jobs") {
            show_jobs = false;
        } else if (arg == "--no-clients") {
            show_clients = false;
        } else if (arg == "--no-stats") {
            show_stats = false;
        } else if (arg == "-h" || arg == "--help") {
            cout << "Usage: monitor [-i|--interval <sec>] [--no-jobs] [--no-clients] [--no-stats]\n";
            cout << "  -i, --interval <sec>  Refresh interval in seconds (default: 5)\n";
            cout << "  --no-jobs            Don't show job information\n";
            cout << "  --no-clients         Don't show client information\n";
            cout << "  --no-stats           Don't show server statistics\n";
            cout << "\nPress 'q' to quit monitoring mode\n";
            return true;
        }
    }
    
    return handle_real_time_monitor();
}

/**
 * @brief Handle real-time monitoring display
 */
bool CommandProcessor::handle_real_time_monitor() {
    cout << "Entering real-time monitoring mode. Press 'q' to quit.\n" << endl;
    
    // Save original terminal settings if we have ncurses
#ifdef HAVE_NCURSES
    if (admin_client->config.use_colors) {
        initscr();
        noecho();
        cbreak();
        nodelay(stdscr, TRUE);
        keypad(stdscr, TRUE);
    }
#endif
    
    bool quit_monitoring = false;
    auto last_refresh = chrono::steady_clock::now();
    
    while (!quit_monitoring && !g_shutdown_requested) {
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(now - last_refresh);
        
        // Check for user input
        char ch = 0;
#ifdef HAVE_NCURSES
        if (admin_client->config.use_colors) {
            ch = getch();
        } else
#endif
        {
            // Simple polling for regular terminal
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            struct timeval timeout = {0, 100000}; // 100ms timeout
            
            if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &timeout) > 0) {
                ch = getchar();
            }
        }
        
        if (ch == 'q' || ch == 'Q') {
            quit_monitoring = true;
            break;
        }
        
        // Refresh display if interval elapsed
        if (elapsed.count() >= admin_client->config.refresh_interval) {
            // Clear screen and display header
#ifdef HAVE_NCURSES
            if (admin_client->config.use_colors) {
                clear();
                mvprintw(0, 0, "=== Code Compiler & Executer - Real-time Monitor ===");
                mvprintw(1, 0, "Press 'q' to quit | Refresh interval: %d seconds", 
                        admin_client->config.refresh_interval);
                mvprintw(2, 0, "Last update: %s", format_current_time().c_str());
                refresh();
            } else
#endif
            {
                cout << "\033[2J\033[H"; // Clear screen and move cursor to top
                cout << "=== Code Compiler & Executer - Real-time Monitor ===" << endl;
                cout << "Press 'q' to quit | Refresh interval: " << admin_client->config.refresh_interval << " seconds" << endl;
                cout << "Last update: " << format_current_time() << endl << endl;
            }
            
            // Fetch and display server stats
            admin_command_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.command_type = 3; // SERVER_STATS
            
            if (send_command_and_wait(MSG_ADMIN_SERVER_STATS, cmd)) {
                // Display would be handled by the response processor
            }
            
            // Fetch and display active jobs
            memset(&cmd, 0, sizeof(cmd));
            cmd.command_type = 2; // LIST_JOBS
            cmd.flags = 1; // Active only
            
            if (send_command_and_wait(MSG_ADMIN_LIST_JOBS, cmd)) {
                // Display would be handled by the response processor
            }
            
            last_refresh = now;
        }
        
        // Small delay to prevent busy waiting
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    
    // Restore terminal
#ifdef HAVE_NCURSES
    if (admin_client->config.use_colors) {
        endwin();
    }
#endif
    
    cout << "\nExited monitoring mode." << endl;
    return true;
}

/**
 * @brief Handle bulk operations
 */
bool CommandProcessor::handle_bulk_disconnect(const vector<string>& args) {
    if (args.empty()) {
        cout << "Usage: bulk_disconnect <criteria>\n";
        cout << "Criteria:\n";
        cout << "  --idle <minutes>     Disconnect clients idle for more than X minutes\n";
        cout << "  --ip <pattern>       Disconnect clients matching IP pattern\n";
        cout << "  --all-except <id>    Disconnect all clients except specified ID\n";
        return true;
    }
    
    string criteria = args[0];
    string value = args.size() > 1 ? args[1] : "";
    
    // Confirm bulk operation
    if (!admin_client->config.batch_mode) {
        cout << "This will disconnect multiple clients. Are you sure? (y/N): ";
        string confirmation;
        getline(cin, confirmation);
        if (confirmation != "y" && confirmation != "Y") {
            cout << "Bulk disconnect cancelled" << endl;
            return true;
        }
    }
    
    admin_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = 10; // BULK_DISCONNECT
    
    if (criteria == "--idle") {
        cmd.flags = 1;
        cmd.target_id = static_cast<uint32_t>(atoi(value.c_str()));
    } else if (criteria == "--ip") {
        cmd.flags = 2;
        strncpy(cmd.command_data, value.c_str(), sizeof(cmd.command_data) - 1);
    } else if (criteria == "--all-except") {
        cmd.flags = 3;
        cmd.target_id = static_cast<uint32_t>(atoi(value.c_str()));
    } else {
        cerr << "Unknown criteria: " << criteria << endl;
        return true;
    }
    
    return send_command_and_wait(MSG_ADMIN_DISCONNECT_CLIENT, cmd);
}

/**
 * @brief Handle log viewing
 */
bool CommandProcessor::handle_logs(const vector<string>& args) {
    string log_type = "server";
    int lines = 50;
    bool follow = false;
    
    // Parse arguments
    for (size_t i = 0; i < args.size(); ++i) {
        const string& arg = args[i];
        
        if (arg == "-n" || arg == "--lines") {
            if (i + 1 < args.size()) {
                lines = atoi(args[++i].c_str());
                if (lines < 1) lines = 1;
            }
        } else if (arg == "-f" || arg == "--follow") {
            follow = true;
        } else if (arg == "--type") {
            if (i + 1 < args.size()) {
                log_type = args[++i];
            }
        } else if (arg == "-h" || arg == "--help") {
            cout << "Usage: logs [-n|--lines <count>] [-f|--follow] [--type <type>]\n";
            cout << "  -n, --lines <count>  Number of lines to show (default: 50)\n";
            cout << "  -f, --follow         Follow log output\n";
            cout << "  --type <type>        Log type (server, compilation, admin)\n";
            return true;
        }
    }
    
    admin_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = 11; // GET_LOGS
    cmd.target_id = lines;
    cmd.flags = follow ? 1 : 0;
    strncpy(cmd.command_data, log_type.c_str(), sizeof(cmd.command_data) - 1);
    
    return send_command_and_wait(MSG_ADMIN_CONFIG_GET, cmd); // Reuse config message type
}

/**
 * @brief Handle report generation
 */
bool CommandProcessor::handle_generate_report(const vector<string>& args) {
    string report_type = "summary";
    string output_file;
    string format = "text";
    
    // Parse arguments
    for (size_t i = 0; i < args.size(); ++i) {
        const string& arg = args[i];
        
        if (arg == "--type") {
            if (i + 1 < args.size()) {
                report_type = args[++i];
            }
        } else if (arg == "--output" || arg == "-o") {
            if (i + 1 < args.size()) {
                output_file = args[++i];
            }
        } else if (arg == "--format") {
            if (i + 1 < args.size()) {
                format = args[++i];
            }
        } else if (arg == "-h" || arg == "--help") {
            cout << "Usage: report [--type <type>] [--output <file>] [--format <fmt>]\n";
            cout << "  --type <type>      Report type (summary, detailed, performance)\n";
            cout << "  --output <file>    Output file (default: stdout)\n";
            cout << "  --format <fmt>     Output format (text, json, csv)\n";
            return true;
        }
    }
    
    cout << "Generating " << report_type << " report..." << endl;
    
    // For now, just show a sample report
    cout << "\n=== Server Report (" << report_type << ") ===" << endl;
    cout << "Generated: " << format_current_time() << endl;
    cout << "Report format: " << format << endl;
    
    if (!output_file.empty()) {
        cout << "Output file: " << output_file << endl;
        // In a real implementation, we would save to file
    }
    
    return true;
}

/**
 * @brief Send command and wait for response
 */
bool CommandProcessor::send_command_and_wait(message_type_t type, const admin_command_t& cmd) {
    message_t msg;
    init_message_header(&msg.header, type, sizeof(cmd), generate_correlation_id());
    msg.data = const_cast<admin_command_t*>(&cmd);
    
    if (!admin_client->send_message(msg)) {
        cerr << "Failed to send command" << endl;
        return false;
    }
    
    message_t response;
    if (!admin_client->receive_message(response)) {
        cerr << "Failed to receive response" << endl;
        return false;
    }
    
    // Let the admin client process the response
    admin_client->process_server_response(response);
    
    // Cleanup
    if (response.data) {
        free(response.data);
    }
    
    return true;
}

/**
 * @brief Display data in table format
 */
void CommandProcessor::display_table(const vector<vector<string>>& data, 
                                    const vector<string>& headers) {
    if (data.empty() || headers.empty()) {
        return;
    }
    
    // Calculate column widths
    vector<size_t> col_widths(headers.size(), 0);
    
    // Check header widths
    for (size_t i = 0; i < headers.size(); ++i) {
        col_widths[i] = headers[i].length();
    }
    
    // Check data widths
    for (const auto& row : data) {
        for (size_t i = 0; i < min(row.size(), col_widths.size()); ++i) {
            col_widths[i] = max(col_widths[i], row[i].length());
        }
    }
    
    // Print headers
    cout << endl;
    for (size_t i = 0; i < headers.size(); ++i) {
        cout << left << setw(col_widths[i] + 2) << headers[i];
    }
    cout << endl;
    
    // Print separator
    for (size_t i = 0; i < headers.size(); ++i) {
        cout << string(col_widths[i] + 2, '-');
    }
    cout << endl;
    
    // Print data rows
    for (const auto& row : data) {
        for (size_t i = 0; i < min(row.size(), col_widths.size()); ++i) {
            cout << left << setw(col_widths[i] + 2) << row[i];
        }
        cout << endl;
    }
    cout << endl;
}

/**
 * @brief Display progress bar
 */
void CommandProcessor::display_progress_bar(int current, int total, const string& label) {
    const int bar_width = 50;
    float percentage = static_cast<float>(current) / total;
    int filled = static_cast<int>(percentage * bar_width);
    
    cout << "\r" << label << " [";
    
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled) {
            cout << "=";
        } else if (i == filled) {
            cout << ">";
        } else {
            cout << " ";
        }
    }
    
    cout << "] " << fixed << setprecision(1) << (percentage * 100.0f) << "% ";
    cout << "(" << current << "/" << total << ")";
    cout.flush();
    
    if (current == total) {
        cout << endl;
    }
}

/**
 * @brief Format current time
 */
string CommandProcessor::format_current_time() {
    time_t now = time(nullptr);
    char buffer[64];
    struct tm* tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return string(buffer);
}

// TerminalUtils implementation
const string TerminalUtils::COLOR_RED = "\033[31m";
const string TerminalUtils::COLOR_GREEN = "\033[32m";
const string TerminalUtils::COLOR_YELLOW = "\033[33m";
const string TerminalUtils::COLOR_BLUE = "\033[34m";
const string TerminalUtils::COLOR_MAGENTA = "\033[35m";
const string TerminalUtils::COLOR_CYAN = "\033[36m";
const string TerminalUtils::COLOR_WHITE = "\033[37m";
const string TerminalUtils::COLOR_RESET = "\033[0m";

const string TerminalUtils::STYLE_BOLD = "\033[1m";
const string TerminalUtils::STYLE_DIM = "\033[2m";
const string TerminalUtils::STYLE_UNDERLINE = "\033[4m";
const string TerminalUtils::STYLE_RESET = "\033[0m";

bool TerminalUtils::supports_colors() {
    const char* term = getenv("TERM");
    return term && (strstr(term, "color") || strstr(term, "xterm") || strstr(term, "screen"));
}

string TerminalUtils::colorize(const string& text, const string& color) {
    if (!supports_colors()) {
        return text;
    }
    return color + text + COLOR_RESET;
}

string TerminalUtils::stylize(const string& text, const string& style) {
    if (!supports_colors()) {
        return text;
    }
    return style + text + STYLE_RESET;
}

string TerminalUtils::center_text(const string& text, int width) {
    if (static_cast<int>(text.length()) >= width) {
        return text;
    }
    
    int padding = (width - text.length()) / 2;
    return string(padding, ' ') + text + string(width - text.length() - padding, ' ');
}

string TerminalUtils::truncate_text(const string& text, int max_width) {
    if (static_cast<int>(text.length()) <= max_width) {
        return text;
    }
    return text.substr(0, max_width - 3) + "...";
}

string TerminalUtils::get_check_mark() {
    return supports_colors() ? colorize("✓", COLOR_GREEN) : "[OK]";
}

string TerminalUtils::get_cross_mark() {
    return supports_colors() ? colorize("✗", COLOR_RED) : "[FAIL]";
}

string TerminalUtils::get_warning_symbol() {
    return supports_colors() ? colorize("⚠", COLOR_YELLOW) : "[WARN]";
}

string TerminalUtils::get_info_symbol() {
    return supports_colors() ? colorize("ℹ", COLOR_BLUE) : "[INFO]";
}

string TerminalUtils::get_progress_bar(int percentage, int width) {
    if (width < 2) width = 20;
    
    int filled = (percentage * (width - 2)) / 100;
    string bar = "[";
    
    for (int i = 0; i < width - 2; ++i) {
        if (i < filled) {
            bar += "=";
        } else if (i == filled && percentage < 100) {
            bar += ">";
        } else {
            bar += " ";
        }
    }
    
    bar += "]";
    return bar;
}

void TerminalUtils::clear_screen() {
    cout << "\033[2J\033[H" << flush;
}

bool TerminalUtils::confirm_yes_no(const string& prompt) {
    cout << prompt << " (y/N): ";
    string response;
    getline(cin, response);
    return (response == "y" || response == "Y" || response == "yes" || response == "YES");
}