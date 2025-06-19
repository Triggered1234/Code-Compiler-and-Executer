/**
 * @file client.h
 * @brief Unix client definitions for Code Compiler & Executer
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"
#include "communication.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

// Version information
#define CLIENT_VERSION "1.0.0"

// Default configuration values
#define DEFAULT_SERVER_HOST "localhost"
#define DEFAULT_SERVER_PORT 8080
#define DEFAULT_TIMEOUT 30
#define DEFAULT_CHUNK_SIZE (64 * 1024)  // 64KB
#define MAX_RETRIES 3

namespace CodeCompiler {

/**
 * @brief Client configuration structure
 */
struct ClientConfig {
    std::string server_host = DEFAULT_SERVER_HOST;
    int server_port = DEFAULT_SERVER_PORT;
    int timeout = DEFAULT_TIMEOUT;
    int max_retries = MAX_RETRIES;
    size_t chunk_size = DEFAULT_CHUNK_SIZE;
    std::string client_name = "Unix Client";
    std::string config_file;
    bool verbose = false;
    bool debug = false;
    bool keep_alive = true;
    bool auto_reconnect = true;
    bool use_compression = false;
    bool use_encryption = false;
    
    // Authentication (if implemented)
    std::string username;
    std::string password;
    std::string api_key;
};

/**
 * @brief Compilation job information
 */
struct CompilationJob {
    uint32_t job_id = 0;
    std::string filename;
    language_t language = LANGUAGE_UNKNOWN;
    execution_mode_t mode = EXEC_MODE_COMPILE_AND_RUN;
    std::string compiler_args;
    std::string execution_args;
    job_status_t status = JOB_STATUS_QUEUED;
    
    // Timing information
    std::chrono::system_clock::time_point submit_time;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // Results
    int exit_code = 0;
    size_t output_size = 0;
    size_t error_size = 0;
    std::string output;
    std::string error_output;
    uint32_t execution_time_ms = 0;
};

/**
 * @brief Progress callback function type
 */
using ProgressCallback = std::function<void(size_t current, size_t total)>;

/**
 * @brief Job completion callback function type
 */
using JobCallback = std::function<void(const CompilationJob& job, bool success)>;

/**
 * @brief Connection event callback function type
 */
using ConnectionCallback = std::function<void(bool connected, const std::string& message)>;

/**
 * @brief Main client class
 */
class CodeCompilerClient {
public:
    explicit CodeCompilerClient(const ClientConfig& config = ClientConfig{});
    ~CodeCompilerClient();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;
    bool reconnect();
    
    // Configuration
    void setConfig(const ClientConfig& config);
    const ClientConfig& getConfig() const;
    bool loadConfig(const std::string& filename);
    bool saveConfig(const std::string& filename);
    
    // File operations
    bool uploadFile(const std::string& filepath, ProgressCallback progress = nullptr);
    bool downloadFile(const std::string& remote_filename, const std::string& local_filepath,
                     ProgressCallback progress = nullptr);
    
    // Compilation operations
    std::shared_ptr<CompilationJob> submitFile(const std::string& filepath,
                                              const std::string& compiler_args = "",
                                              const std::string& execution_args = "",
                                              execution_mode_t mode = EXEC_MODE_COMPILE_AND_RUN);
    
    bool getJobStatus(uint32_t job_id, CompilationJob& job);
    bool getJobResults(uint32_t job_id, CompilationJob& job);
    bool cancelJob(uint32_t job_id);
    
    // Convenience methods
    bool compileAndRun(const std::string& filepath,
                      const std::string& compiler_args = "",
                      const std::string& execution_args = "",
                      bool wait_for_completion = true);
    
    bool compileOnly(const std::string& filepath,
                    const std::string& compiler_args = "");
    
    bool interpretFile(const std::string& filepath,
                      const std::string& args = "");
    
    bool checkSyntax(const std::string& filepath);
    
    // Async operations
    void submitFileAsync(const std::string& filepath,
                        JobCallback callback,
                        const std::string& compiler_args = "",
                        const std::string& execution_args = "",
                        execution_mode_t mode = EXEC_MODE_COMPILE_AND_RUN);
    
    bool waitForJob(uint32_t job_id, std::chrono::seconds timeout = std::chrono::seconds(300));
    bool waitForJob(const CompilationJob& job, std::chrono::seconds timeout = std::chrono::seconds(300));
    
    // Server communication
    bool pingServer();
    bool testConnection();
    
    // Utility methods
    language_t detectLanguage(const std::string& filepath) const;
    std::string getLanguageName(language_t language) const;
    std::string getExecutionModeName(execution_mode_t mode) const;
    std::string getJobStatusName(job_status_t status) const;
    
    // Event callbacks
    void setConnectionCallback(ConnectionCallback callback);
    void setDefaultJobCallback(JobCallback callback);
    
    // Error handling
    std::string getLastError() const;
    int getLastErrorCode() const;
    void clearError();
    
    // Statistics
    struct Statistics {
        size_t files_uploaded = 0;
        size_t files_downloaded = 0;
        size_t bytes_uploaded = 0;
        size_t bytes_downloaded = 0;
        size_t jobs_submitted = 0;
        size_t jobs_completed = 0;
        size_t jobs_failed = 0;
        std::chrono::milliseconds total_upload_time{0};
        std::chrono::milliseconds total_download_time{0};
        std::chrono::system_clock::time_point session_start;
    };
    
    const Statistics& getStatistics() const;
    void resetStatistics();
    void printStatistics() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Client factory functions
 */
std::unique_ptr<CodeCompilerClient> createClient(const ClientConfig& config = ClientConfig{});
std::unique_ptr<CodeCompilerClient> createClientFromConfig(const std::string& config_file);

/**
 * @brief Configuration management utilities
 */
namespace Config {
    bool loadFromFile(const std::string& filename, ClientConfig& config);
    bool saveToFile(const std::string& filename, const ClientConfig& config);
    ClientConfig getDefaultConfig();
    bool validateConfig(const ClientConfig& config);
    std::string getConfigDir();
    std::string getDefaultConfigPath();
}

/**
 * @brief File utilities
 */
namespace FileUtils {
    bool exists(const std::string& path);
    bool isFile(const std::string& path);
    bool isReadable(const std::string& path);
    size_t getSize(const std::string& path);
    std::string getExtension(const std::string& path);
    std::string getBasename(const std::string& path);
    std::string getDirname(const std::string& path);
    language_t detectLanguageFromPath(const std::string& path);
    bool validateSourceFile(const std::string& path);
    std::string generateTempPath(const std::string& prefix = "client_");
}

/**
 * @brief Network utilities
 */
namespace NetUtils {
    bool isValidHostname(const std::string& hostname);
    bool isValidPort(int port);
    bool isHostReachable(const std::string& hostname, int port, int timeout_ms = 5000);
    std::string resolveHostname(const std::string& hostname);
    std::string getLocalIP();
}

/**
 * @brief String utilities
 */
namespace StringUtils {
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string join(const std::vector<std::string>& strings, const std::string& delimiter);
    std::string trim(const std::string& str);
    std::string toLower(const std::string& str);
    std::string toUpper(const std::string& str);
    bool startsWith(const std::string& str, const std::string& prefix);
    bool endsWith(const std::string& str, const std::string& suffix);
    std::string formatBytes(size_t bytes);
    std::string formatDuration(std::chrono::milliseconds duration);
    std::string formatTimestamp(const std::chrono::system_clock::time_point& time);
}

/**
 * @brief Command line argument parsing
 */
class ArgumentParser {
public:
    ArgumentParser(const std::string& program_name, const std::string& description = "");
    
    void addArgument(const std::string& name, const std::string& help,
                    bool required = false, const std::string& default_value = "");
    void addFlag(const std::string& name, const std::string& help);
    void addOption(const std::string& short_name, const std::string& long_name,
                  const std::string& help, bool has_value = true);
    
    bool parse(int argc, char* argv[]);
    void printHelp() const;
    void printVersion() const;
    
    bool hasArgument(const std::string& name) const;
    std::string getArgument(const std::string& name) const;
    bool getFlag(const std::string& name) const;
    
    std::vector<std::string> getPositionalArgs() const;

private:
    struct Argument;
    std::vector<std::unique_ptr<Argument>> arguments_;
    std::vector<std::string> positional_args_;
    std::string program_name_;
    std::string description_;
};

/**
 * @brief Interactive client shell
 */
class InteractiveShell {
public:
    explicit InteractiveShell(CodeCompilerClient& client);
    
    void run();
    void stop();
    
    void addCommand(const std::string& name, const std::string& help,
                   std::function<bool(const std::vector<std::string>&)> handler);
    
    void setPrompt(const std::string& prompt);
    void enableHistory(bool enable);
    void enableCompletion(bool enable);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Exception classes
 */
class ClientException : public std::exception {
public:
    explicit ClientException(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }

private:
    std::string message_;
};

class ConnectionException : public ClientException {
public:
    explicit ConnectionException(const std::string& message) 
        : ClientException("Connection error: " + message) {}
};

class ProtocolException : public ClientException {
public:
    explicit ProtocolException(const std::string& message)
        : ClientException("Protocol error: " + message) {}
};

class FileException : public ClientException {
public:
    explicit FileException(const std::string& message)
        : ClientException("File error: " + message) {}
};

/**
 * @brief Logging utilities
 */
namespace Log {
    enum Level {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4
    };
    
    void setLevel(Level level);
    void setOutput(const std::string& filename);
    void enableConsole(bool enable);
    void enableColors(bool enable);
    
    void trace(const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
}

/**
 * @brief Global utility functions
 */
std::string getClientVersion();
std::string getBuildInfo();
void printVersionInfo();
void printUsage(const std::string& program_name);

} // namespace CodeCompiler

#endif // CLIENT_H