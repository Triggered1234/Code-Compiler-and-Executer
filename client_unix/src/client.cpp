/**
 * @file client.cpp
 * @brief Main Unix client implementation for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#include "client.h"
#include "communication.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>
#include <signal.h>

using namespace CodeCompiler;

// Global variables for signal handling
static volatile bool g_interrupted = false;

/**
 * @brief Signal handler for graceful shutdown
 */
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_interrupted = true;
        std::cout << "\nInterrupted by user. Shutting down..." << std::endl;
    }
}

/**
 * @brief Setup signal handling
 */
void setupSignalHandling() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe
}

/**
 * @brief Print usage information
 */
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] [COMMAND]\n\n";
    std::cout << "Code Compiler & Executer Unix Client\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "  -h, --host HOST        Server hostname (default: localhost)\n";
    std::cout << "  -p, --port PORT        Server port (default: 8080)\n";
    std::cout << "  -t, --timeout SEC      Connection timeout in seconds (default: 30)\n";
    std::cout << "  -c, --config FILE      Configuration file\n";
    std::cout << "  -v, --verbose          Verbose output\n";
    std::cout << "  -d, --debug            Debug output\n";
    std::cout << "  --help                 Show this help\n";
    std::cout << "  --version              Show version information\n";
    std::cout << "\nCOMMANDS:\n";
    std::cout << "  compile FILE [ARGS]    Compile source file\n";
    std::cout << "  run FILE [ARGS]        Compile and run source file\n";
    std::cout << "  interpret FILE [ARGS]  Interpret source file\n";
    std::cout << "  check FILE             Check syntax only\n";
    std::cout << "  ping                   Test server connectivity\n";
    std::cout << "  interactive            Start interactive mode\n";
    std::cout << "\nEXAMPLES:\n";
    std::cout << "  " << program_name << " run hello.cpp\n";
    std::cout << "  " << program_name << " compile -O2 program.c\n";
    std::cout << "  " << program_name << " -h server.example.com run script.py\n";
    std::cout << "  " << program_name << " --config client.conf interactive\n";
    std::cout << std::endl;
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << "Code Compiler & Executer Unix Client " << CLIENT_VERSION << std::endl;
    std::cout << "Built on " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << "Authors: Rares-Nicholas Popa & Adrian-Petru Enache" << std::endl;
}

/**
 * @brief Parse command line arguments
 */
bool parseArguments(int argc, char* argv[], ClientConfig& config, 
                   std::string& command, std::vector<std::string>& args) {
    int opt;
    const char* short_options = "h:p:t:c:vd";
    const struct option long_options[] = {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"timeout", required_argument, 0, 't'},
        {"config", required_argument, 0, 'c'},
        {"verbose", no_argument, 0, 'v'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 1000},
        {"version", no_argument, 0, 1001},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'h':
                config.server_host = optarg;
                break;
            case 'p':
                config.server_port = std::atoi(optarg);
                if (config.server_port <= 0 || config.server_port > 65535) {
                    std::cerr << "Error: Invalid port number: " << optarg << std::endl;
                    return false;
                }
                break;
            case 't':
                config.timeout = std::atoi(optarg);
                if (config.timeout <= 0) {
                    std::cerr << "Error: Invalid timeout: " << optarg << std::endl;
                    return false;
                }
                break;
            case 'c':
                config.config_file = optarg;
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'd':
                config.debug = true;
                config.verbose = true;  // Debug implies verbose
                break;
            case 1000:  // --help
                printUsage(argv[0]);
                exit(0);
            case 1001:  // --version
                printVersion();
                exit(0);
            default:
                printUsage(argv[0]);
                return false;
        }
    }
    
    // Parse remaining arguments as command and parameters
    if (optind < argc) {
        command = argv[optind++];
        while (optind < argc) {
            args.push_back(argv[optind++]);
        }
    }
    
    return true;
}

/**
 * @brief Load configuration from file
 */
bool loadConfig(const std::string& filename, ClientConfig& config) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        line = StringUtils::trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse key=value pairs
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        
        std::string key = StringUtils::trim(line.substr(0, pos));
        std::string value = StringUtils::trim(line.substr(pos + 1));
        
        // Apply configuration values
        if (key == "host") {
            config.server_host = value;
        } else if (key == "port") {
            config.server_port = std::atoi(value.c_str());
        } else if (key == "timeout") {
            config.timeout = std::atoi(value.c_str());
        } else if (key == "verbose") {
            config.verbose = (value == "true" || value == "1");
        } else if (key == "debug") {
            config.debug = (value == "true" || value == "1");
        } else if (key == "client_name") {
            config.client_name = value;
        }
    }
    
    return true;
}

/**
 * @brief Display progress for file operations
 */
class ProgressDisplay {
public:
    ProgressDisplay(const std::string& operation, bool verbose)
        : operation_(operation), verbose_(verbose), last_percent_(0) {
        if (verbose_) {
            std::cout << operation_ << ": 0%" << std::flush;
        }
    }
    
    ~ProgressDisplay() {
        if (verbose_ && last_percent_ > 0) {
            std::cout << std::endl;
        }
    }
    
    void update(size_t current, size_t total) {
        if (!verbose_ || total == 0) return;
        
        int percent = static_cast<int>((current * 100) / total);
        if (percent != last_percent_) {
            std::cout << "\r" << operation_ << ": " << percent << "%" << std::flush;
            last_percent_ = percent;
        }
    }
    
    void complete() {
        if (verbose_) {
            std::cout << "\r" << operation_ << ": 100% - Complete" << std::endl;
        }
    }

private:
    std::string operation_;
    bool verbose_;
    int last_percent_;
};

/**
 * @brief Execute compile command
 */
int executeCompile(CodeCompilerClient& client, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Error: No source file specified" << std::endl;
        return 1;
    }
    
    std::string filename = args[0];
    std::string compiler_args;
    
    // Join remaining arguments as compiler arguments
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) compiler_args += " ";
        compiler_args += args[i];
    }
    
    if (client.getConfig().verbose) {
        std::cout << "Compiling " << filename;
        if (!compiler_args.empty()) {
            std::cout << " with args: " << compiler_args;
        }
        std::cout << std::endl;
    }
    
    bool success = client.compileOnly(filename, compiler_args);
    
    if (success) {
        std::cout << "Compilation successful" << std::endl;
        return 0;
    } else {
        std::cerr << "Compilation failed: " << client.getLastError() << std::endl;
        return 1;
    }
}

/**
 * @brief Execute run command
 */
int executeRun(CodeCompilerClient& client, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Error: No source file specified" << std::endl;
        return 1;
    }
    
    std::string filename = args[0];
    std::string compiler_args;
    std::string execution_args;
    
    // Parse arguments - everything after -- goes to execution
    bool execution_mode = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--") {
            execution_mode = true;
            continue;
        }
        
        if (execution_mode) {
            if (!execution_args.empty()) execution_args += " ";
            execution_args += args[i];
        } else {
            if (!compiler_args.empty()) compiler_args += " ";
            compiler_args += args[i];
        }
    }
    
    if (client.getConfig().verbose) {
        std::cout << "Compiling and running " << filename;
        if (!compiler_args.empty()) {
            std::cout << " (compiler args: " << compiler_args << ")";
        }
        if (!execution_args.empty()) {
            std::cout << " (execution args: " << execution_args << ")";
        }
        std::cout << std::endl;
    }
    
    bool success = client.compileAndRun(filename, compiler_args, execution_args);
    
    if (success) {
        if (client.getConfig().verbose) {
            std::cout << "Execution completed successfully" << std::endl;
        }
        return 0;
    } else {
        std::cerr << "Execution failed: " << client.getLastError() << std::endl;
        return 1;
    }
}

/**
 * @brief Execute interpret command
 */
int executeInterpret(CodeCompilerClient& client, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Error: No source file specified" << std::endl;
        return 1;
    }
    
    std::string filename = args[0];
    std::string execution_args;
    
    // Join remaining arguments as execution arguments
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) execution_args += " ";
        execution_args += args[i];
    }
    
    if (client.getConfig().verbose) {
        std::cout << "Interpreting " << filename;
        if (!execution_args.empty()) {
            std::cout << " with args: " << execution_args;
        }
        std::cout << std::endl;
    }
    
    bool success = client.interpretFile(filename, execution_args);
    
    if (success) {
        if (client.getConfig().verbose) {
            std::cout << "Interpretation completed successfully" << std::endl;
        }
        return 0;
    } else {
        std::cerr << "Interpretation failed: " << client.getLastError() << std::endl;
        return 1;
    }
}

/**
 * @brief Execute syntax check command
 */
int executeCheck(CodeCompilerClient& client, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Error: No source file specified" << std::endl;
        return 1;
    }
    
    std::string filename = args[0];
    
    if (client.getConfig().verbose) {
        std::cout << "Checking syntax of " << filename << std::endl;
    }
    
    bool success = client.checkSyntax(filename);
    
    if (success) {
        std::cout << "Syntax check passed" << std::endl;
        return 0;
    } else {
        std::cerr << "Syntax check failed: " << client.getLastError() << std::endl;
        return 1;
    }
}

/**
 * @brief Execute ping command
 */
int executePing(CodeCompilerClient& client, const std::vector<std::string>&) {
    if (client.getConfig().verbose) {
        std::cout << "Pinging server..." << std::endl;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    bool success = client.pingServer();
    auto end = std::chrono::high_resolution_clock::now();
    
    if (success) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Server responded in " << duration.count() << " ms" << std::endl;
        return 0;
    } else {
        std::cerr << "Server is not responding: " << client.getLastError() << std::endl;
        return 1;
    }
}

/**
 * @brief Execute interactive mode
 */
int executeInteractive(CodeCompilerClient& client, const std::vector<std::string>&) {
    std::cout << "Code Compiler & Executer Interactive Mode" << std::endl;
    std::cout << "Type 'help' for available commands, 'quit' to exit" << std::endl;
    
    std::string line;
    while (!g_interrupted) {
        std::cout << "client> ";
        if (!std::getline(std::cin, line)) {
            break;  // EOF
        }
        
        line = StringUtils::trim(line);
        if (line.empty()) {
            continue;
        }
        
        if (line == "quit" || line == "exit") {
            break;
        } else if (line == "help") {
            std::cout << "Available commands:\n";
            std::cout << "  compile <file> [args]  - Compile source file\n";
            std::cout << "  run <file> [args]      - Compile and run source file\n";
            std::cout << "  interpret <file> [args]- Interpret source file\n";
            std::cout << "  check <file>           - Check syntax\n";
            std::cout << "  ping                   - Test server connectivity\n";
            std::cout << "  stats                  - Show client statistics\n";
            std::cout << "  quit/exit              - Exit interactive mode\n";
        } else if (line == "ping") {
            executePing(client, {});
        } else if (line == "stats") {
            client.printStatistics();
        } else {
            // Parse command and arguments
            std::vector<std::string> tokens = StringUtils::split(line, ' ');
            if (!tokens.empty()) {
                std::string cmd = tokens[0];
                std::vector<std::string> cmd_args(tokens.begin() + 1, tokens.end());
                
                if (cmd == "compile") {
                    executeCompile(client, cmd_args);
                } else if (cmd == "run") {
                    executeRun(client, cmd_args);
                } else if (cmd == "interpret") {
                    executeInterpret(client, cmd_args);
                } else if (cmd == "check") {
                    executeCheck(client, cmd_args);
                } else {
                    std::cerr << "Unknown command: " << cmd << std::endl;
                    std::cerr << "Type 'help' for available commands" << std::endl;
                }
            }
        }
    }
    
    return 0;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    setupSignalHandling();
    
    // Parse command line arguments
    ClientConfig config;
    std::string command;
    std::vector<std::string> args;
    
    if (!parseArguments(argc, argv, config, command, args)) {
        return 1;
    }
    
    // Load configuration file if specified
    if (!config.config_file.empty()) {
        loadConfig(config.config_file, config);
    }
    
    // Create and configure client
    CodeCompilerClient client(config);
    
    // Setup progress callback if verbose
    if (config.verbose) {
        client.setConnectionCallback([](bool connected, const std::string& message) {
            if (connected) {
                std::cout << "Connected to server: " << message << std::endl;
            } else {
                std::cout << "Disconnected from server: " << message << std::endl;
            }
        });
        
        client.setDefaultJobCallback([](const CompilationJob& job, bool success) {
            if (success) {
                std::cout << "Job " << job.job_id << " completed successfully" << std::endl;
            } else {
                std::cout << "Job " << job.job_id << " failed" << std::endl;
            }
        });
    }
    
    try {
        // Connect to server
        if (config.verbose) {
            std::cout << "Connecting to " << config.server_host << ":" << config.server_port << "..." << std::endl;
        }
        
        if (!client.connect()) {
            std::cerr << "Failed to connect to server: " << client.getLastError() << std::endl;
            return 1;
        }
        
        if (config.verbose) {
            std::cout << "Connected successfully!" << std::endl;
        }
        
        // Execute command
        int result = 0;
        
        if (command.empty() || command == "interactive") {
            result = executeInteractive(client, args);
        } else if (command == "compile") {
            result = executeCompile(client, args);
        } else if (command == "run") {
            result = executeRun(client, args);
        } else if (command == "interpret") {
            result = executeInterpret(client, args);
        } else if (command == "check") {
            result = executeCheck(client, args);
        } else if (command == "ping") {
            result = executePing(client, args);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            printUsage(argv[0]);
            result = 1;
        }
        
        // Print statistics if verbose and not in interactive mode
        if (config.verbose && command != "interactive") {
            std::cout << "\n=== Session Statistics ===" << std::endl;
            client.printStatistics();
        }
        
        // Disconnect
        if (config.verbose) {
            std::cout << "Disconnecting..." << std::endl;
        }
        client.disconnect();
        
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}

// Utility function implementations

namespace CodeCompiler {

std::string getClientVersion() {
    return CLIENT_VERSION;
}

std::string getBuildInfo() {
    std::ostringstream oss;
    oss << "Built on " << __DATE__ << " " << __TIME__;
#ifdef DEBUG
    oss << " (Debug)";
#else
    oss << " (Release)";
#endif
    return oss.str();
}

void printVersionInfo() {
    std::cout << "Code Compiler & Executer Unix Client " << getClientVersion() << std::endl;
    std::cout << getBuildInfo() << std::endl;
    std::cout << "Authors: Rares-Nicholas Popa & Adrian-Petru Enache" << std::endl;
}

namespace StringUtils {

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string join(const std::vector<std::string>& strings, const std::string& delimiter) {
    if (strings.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    oss << strings[0];
    
    for (size_t i = 1; i < strings.size(); ++i) {
        oss << delimiter << strings[i];
    }
    
    return oss.str();
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

bool startsWith(const std::string& str, const std::string& prefix) {
    return str.length() >= prefix.length() && 
           str.compare(0, prefix.length(), prefix) == 0;
}

bool endsWith(const std::string& str, const std::string& suffix) {
    return str.length() >= suffix.length() && 
           str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string formatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return oss.str();
}

std::string formatDuration(std::chrono::milliseconds duration) {
    auto ms = duration.count();
    
    if (ms < 1000) {
        return std::to_string(ms) + " ms";
    } else if (ms < 60000) {
        return std::to_string(ms / 1000) + "." + std::to_string((ms % 1000) / 100) + " s";
    } else {
        auto minutes = ms / 60000;
        auto seconds = (ms % 60000) / 1000;
        return std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    }
}

std::string formatTimestamp(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace StringUtils

namespace FileUtils {

bool exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool isFile(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode));
}

bool isReadable(const std::string& path) {
    return access(path.c_str(), R_OK) == 0;
}

size_t getSize(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) == 0) {
        return static_cast<size_t>(buffer.st_size);
    }
    return 0;
}

std::string getExtension(const std::string& path) {
    size_t pos = path.find_last_of('.');
    if (pos != std::string::npos && pos < path.length() - 1) {
        return path.substr(pos);
    }
    return "";
}

std::string getBasename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

std::string getDirname(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return ".";
}

language_t detectLanguageFromPath(const std::string& path) {
    std::string ext = StringUtils::toLower(getExtension(path));
    
    if (ext == ".c") return LANGUAGE_C;
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return LANGUAGE_CPP;
    if (ext == ".java") return LANGUAGE_JAVA;
    if (ext == ".py") return LANGUAGE_PYTHON;
    if (ext == ".js") return LANGUAGE_JAVASCRIPT;
    if (ext == ".go") return LANGUAGE_GO;
    if (ext == ".rs") return LANGUAGE_RUST;
    
    return LANGUAGE_UNKNOWN;
}

bool validateSourceFile(const std::string& path) {
    if (!exists(path) || !isFile(path) || !isReadable(path)) {
        return false;
    }
    
    size_t size = getSize(path);
    if (size == 0 || size > 10 * 1024 * 1024) {  // Max 10MB
        return false;
    }
    
    language_t lang = detectLanguageFromPath(path);
    if (lang == LANGUAGE_UNKNOWN) {
        return false;
    }
    
    return true;
}

std::string generateTempPath(const std::string& prefix) {
    std::ostringstream oss;
    oss << "/tmp/" << prefix << "_" << getpid() << "_" << time(nullptr);
    return oss.str();
}

} // namespace FileUtils

namespace NetUtils {

bool isValidHostname(const std::string& hostname) {
    if (hostname.empty() || hostname.length() > 255) {
        return false;
    }
    
    // Basic hostname validation
    for (char c : hostname) {
        if (!std::isalnum(c) && c != '.' && c != '-') {
            return false;
        }
    }
    
    return true;
}

bool isValidPort(int port) {
    return port > 0 && port <= 65535;
}

bool isHostReachable(const std::string& hostname, int port, int timeout_ms) {
    // This is a simplified implementation
    // In a real implementation, you would create a socket and attempt to connect
    UNUSED_PARAM(hostname);
    UNUSED_PARAM(port);
    UNUSED_PARAM(timeout_ms);
    return true;  // Placeholder
}

std::string resolveHostname(const std::string& hostname) {
    // Placeholder implementation
    // In a real implementation, you would use getaddrinfo() or similar
    return hostname;
}

std::string getLocalIP() {
    // Placeholder implementation
    // In a real implementation, you would enumerate network interfaces
    return "127.0.0.1";
}

} // namespace NetUtils

} // namespace CodeCompiler