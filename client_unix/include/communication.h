/**
 * @file communication.h
 * @brief Communication layer for Unix client
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "protocol.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>

namespace CodeCompiler {

/**
 * @brief Message structure for C++ client
 */
class Message {
public:
    Message(message_type_t type, const std::vector<uint8_t>& data = {},
           uint32_t correlation_id = 0);
    
    // Getters
    message_type_t getType() const { return header_.message_type; }
    uint32_t getCorrelationId() const { return header_.correlation_id; }
    uint64_t getTimestamp() const { return header_.timestamp; }
    const std::vector<uint8_t>& getData() const { return data_; }
    size_t getDataSize() const { return data_.size(); }
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static std::unique_ptr<Message> deserialize(const std::vector<uint8_t>& buffer);
    
    // Validation
    bool isValid() const;
    bool validateChecksum() const;

private:
    message_header_t header_;
    std::vector<uint8_t> data_;
    
    void calculateChecksum();
    static uint32_t calculateHeaderChecksum(const message_header_t& header);
};

/**
 * @brief Socket wrapper class
 */
class Socket {
public:
    Socket();
    ~Socket();
    
    // Connection management
    bool connect(const std::string& host, int port, int timeout_ms = 30000);
    void disconnect();
    bool isConnected() const;
    
    // Data transmission
    bool send(const std::vector<uint8_t>& data);
    bool receive(std::vector<uint8_t>& data, size_t expected_size);
    bool sendAll(const std::vector<uint8_t>& data);
    bool receiveAll(std::vector<uint8_t>& data, size_t size);
    
    // Socket options
    bool setTimeout(int timeout_ms);
    bool setKeepAlive(bool enable);
    bool setNonBlocking(bool enable);
    
    // Status
    std::string getLastError() const;
    int getLastErrorCode() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Message handler for protocol communication
 */
class MessageHandler {
public:
    explicit MessageHandler(std::unique_ptr<Socket> socket);
    ~MessageHandler();
    
    // Message operations
    bool sendMessage(const Message& message);
    std::unique_ptr<Message> receiveMessage(int timeout_ms = 30000);
    std::unique_ptr<Message> sendAndReceive(const Message& message, int timeout_ms = 30000);
    
    // Protocol messages
    bool sendHello(const std::string& client_name, const std::string& platform);
    std::unique_ptr<Message> receiveHelloResponse();
    
    bool sendFileUploadStart(const std::string& filename, size_t file_size, 
                           uint32_t chunk_count, size_t chunk_size, uint32_t checksum);
    bool sendFileChunk(uint32_t chunk_id, const std::vector<uint8_t>& chunk_data);
    bool sendFileUploadEnd();
    
    bool sendCompileRequest(const std::string& filename, language_t language,
                          execution_mode_t mode, const std::string& compiler_args,
                          const std::string& execution_args);
    std::unique_ptr<Message> receiveCompileResponse();
    
    bool sendStatusRequest(uint32_t job_id);
    std::unique_ptr<Message> receiveStatusResponse();
    
    bool sendResultRequest(uint32_t job_id);
    std::unique_ptr<Message> receiveResultResponse();
    
    bool sendPing();
    std::unique_ptr<Message> receivePong();
    
    // Utility
    uint32_t generateCorrelationId();
    void setVerbose(bool verbose) { verbose_ = verbose; }
    
    // Error handling
    std::string getLastError() const;
    int getLastErrorCode() const;

private:
    std::unique_ptr<Socket> socket_;
    uint32_t correlation_counter_;
    std::mutex correlation_mutex_;
    bool verbose_;
    mutable std::string last_error_;
    mutable int last_error_code_;
    
    void setError(const std::string& error, int code = 0) const;
};

/**
 * @brief Asynchronous message handler
 */
class AsyncMessageHandler {
public:
    using MessageCallback = std::function<void(std::unique_ptr<Message>)>;
    using ErrorCallback = std::function<void(const std::string&, int)>;
    
    explicit AsyncMessageHandler(std::unique_ptr<Socket> socket);
    ~AsyncMessageHandler();
    
    // Lifecycle
    bool start();
    void stop();
    bool isRunning() const;
    
    // Message operations
    bool sendMessage(const Message& message);
    void sendMessageAsync(const Message& message, MessageCallback response_callback,
                         ErrorCallback error_callback = nullptr,
                         int timeout_ms = 30000);
    
    // Callbacks
    void setDefaultMessageCallback(MessageCallback callback);
    void setDefaultErrorCallback(ErrorCallback callback);
    
    // Statistics
    struct Stats {
        size_t messages_sent = 0;
        size_t messages_received = 0;
        size_t errors = 0;
        std::chrono::milliseconds total_response_time{0};
        std::chrono::system_clock::time_point start_time;
    };
    
    const Stats& getStats() const;
    void resetStats();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief File transfer manager
 */
class FileTransferManager {
public:
    using ProgressCallback = std::function<void(size_t transferred, size_t total)>;
    
    explicit FileTransferManager(MessageHandler& message_handler);
    
    // Upload operations
    bool uploadFile(const std::string& local_path, const std::string& remote_name = "",
                   ProgressCallback progress = nullptr);
    bool uploadFileAsync(const std::string& local_path, 
                        std::function<void(bool)> completion_callback,
                        const std::string& remote_name = "",
                        ProgressCallback progress = nullptr);
    
    // Download operations (placeholder for future implementation)
    bool downloadFile(const std::string& remote_name, const std::string& local_path,
                     ProgressCallback progress = nullptr);
    
    // Configuration
    void setChunkSize(size_t chunk_size);
    size_t getChunkSize() const;
    
    void setCompressionEnabled(bool enabled);
    bool isCompressionEnabled() const;
    
    // Statistics
    struct TransferStats {
        size_t files_uploaded = 0;
        size_t files_downloaded = 0;
        size_t bytes_uploaded = 0;
        size_t bytes_downloaded = 0;
        size_t upload_errors = 0;
        size_t download_errors = 0;
        std::chrono::milliseconds total_upload_time{0};
        std::chrono::milliseconds total_download_time{0};
    };
    
    const TransferStats& getStats() const;
    void resetStats();

private:
    MessageHandler& message_handler_;
    size_t chunk_size_;
    bool compression_enabled_;
    mutable TransferStats stats_;
    
    uint32_t calculateFileChecksum(const std::string& filepath) const;
    bool validateFile(const std::string& filepath) const;
    std::vector<uint8_t> readFileChunk(std::ifstream& file, size_t chunk_size) const;
};

/**
 * @brief Connection manager with reconnection support
 */
class ConnectionManager {
public:
    struct Config {
        std::string host = "localhost";
        int port = 8080;
        int connect_timeout_ms = 30000;
        int response_timeout_ms = 30000;
        bool auto_reconnect = true;
        int max_reconnect_attempts = 3;
        int reconnect_delay_ms = 1000;
        bool keep_alive = true;
    };
    
    using ConnectionCallback = std::function<void(bool connected, const std::string& message)>;
    
    explicit ConnectionManager(const Config& config);
    ~ConnectionManager();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;
    bool reconnect();
    
    // Message handling
    std::unique_ptr<MessageHandler> getMessageHandler();
    std::unique_ptr<AsyncMessageHandler> getAsyncMessageHandler();
    
    // Configuration
    void setConfig(const Config& config);
    const Config& getConfig() const;
    
    // Callbacks
    void setConnectionCallback(ConnectionCallback callback);
    
    // Status
    std::string getConnectionStatus() const;
    std::chrono::system_clock::time_point getConnectTime() const;
    std::chrono::milliseconds getConnectionDuration() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Message serialization utilities
 */
namespace MessageUtils {
    // Hello message
    std::vector<uint8_t> serializeHelloPayload(const std::string& client_name,
                                               const std::string& platform,
                                               uint16_t version_major = 1,
                                               uint16_t version_minor = 0,
                                               uint16_t version_patch = 0,
                                               uint16_t capabilities = 0);
    
    bool deserializeHelloPayload(const std::vector<uint8_t>& data,
                                std::string& server_name, std::string& platform,
                                uint16_t& version_major, uint16_t& version_minor,
                                uint16_t& version_patch, uint16_t& capabilities);
    
    // File upload messages
    std::vector<uint8_t> serializeFileUploadStart(const std::string& filename,
                                                  uint64_t file_size,
                                                  uint32_t chunk_count,
                                                  uint32_t chunk_size,
                                                  uint32_t checksum);
    
    std::vector<uint8_t> serializeFileChunk(uint32_t chunk_id,
                                           const std::vector<uint8_t>& chunk_data);
    
    // Compile request
    std::vector<uint8_t> serializeCompileRequest(const std::string& filename,
                                                language_t language,
                                                execution_mode_t mode,
                                                const std::string& compiler_args,
                                                const std::string& execution_args,
                                                uint16_t flags = 0,
                                                uint16_t priority = 5);
    
    bool deserializeCompileResponse(const std::vector<uint8_t>& data,
                                   uint32_t& job_id, uint16_t& status,
                                   int32_t& exit_code, uint32_t& output_size,
                                   uint32_t& error_size, uint32_t& execution_time_ms);
    
    // Status request/response
    std::vector<uint8_t> serializeStatusRequest(uint32_t job_id);
    
    bool deserializeStatusResponse(const std::vector<uint8_t>& data,
                                  uint32_t& job_id, uint16_t& status,
                                  uint16_t& progress, uint64_t& start_time,
                                  uint64_t& end_time, int32_t& pid,
                                  std::string& status_message);
    
    // Result request/response
    std::vector<uint8_t> serializeResultRequest(uint32_t job_id);
    
    bool deserializeResultResponse(const std::vector<uint8_t>& data,
                                  uint32_t& job_id, uint16_t& status,
                                  int32_t& exit_code, uint32_t& output_size,
                                  uint32_t& error_size, uint32_t& execution_time_ms);
    
    // Error response
    bool deserializeErrorResponse(const std::vector<uint8_t>& data,
                                 uint32_t& error_code, uint32_t& error_line,
                                 std::string& error_message, std::string& error_context);
    
    // Utility functions
    std::string messageTypeToString(message_type_t type);
    std::string languageToString(language_t language);
    std::string executionModeToString(execution_mode_t mode);
    std::string jobStatusToString(job_status_t status);
    std::string errorCodeToString(uint32_t error_code);
    
    language_t stringToLanguage(const std::string& language);
    execution_mode_t stringToExecutionMode(const std::string& mode);
}

/**
 * @brief Network utilities
 */
namespace NetworkUtils {
    // Hostname resolution
    std::vector<std::string> resolveHostname(const std::string& hostname);
    bool isValidIPv4(const std::string& ip);
    bool isValidIPv6(const std::string& ip);
    bool isValidHostname(const std::string& hostname);
    bool isValidPort(int port);
    
    // Connectivity testing
    bool isHostReachable(const std::string& host, int port, int timeout_ms = 5000);
    bool pingHost(const std::string& host, int timeout_ms = 5000);
    
    // Network interface information
    std::vector<std::string> getLocalIPs();
    std::string getDefaultRoute();
    
    // Socket utilities
    bool setSocketOption(int socket_fd, int level, int option, const void* value, size_t size);
    bool getSocketOption(int socket_fd, int level, int option, void* value, size_t* size);
    std::string getSocketError(int socket_fd);
}

/**
 * @brief Protocol validation utilities
 */
namespace ProtocolUtils {
    // Message validation
    bool validateMessageHeader(const message_header_t& header);
    bool validateMessageType(message_type_t type);
    bool validateLanguage(language_t language);
    bool validateExecutionMode(execution_mode_t mode);
    bool validateJobStatus(job_status_t status);
    
    // Data validation
    bool validateFilename(const std::string& filename);
    bool validateCompilerArgs(const std::string& args);
    bool validateFileSize(size_t size);
    bool validateChunkSize(size_t size);
    
    // Protocol constants
    constexpr uint32_t PROTOCOL_MAGIC = 0x43434545;
    constexpr size_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;  // 16MB
    constexpr size_t MAX_FILENAME_SIZE = 256;
    constexpr size_t MAX_COMMAND_SIZE = 1024;
    constexpr size_t HEADER_SIZE = 32;
    
    // Version information
    struct ProtocolVersion {
        uint16_t major = 1;
        uint16_t minor = 0;
        uint16_t patch = 0;
    };
    
    ProtocolVersion getCurrentVersion();
    bool isVersionCompatible(const ProtocolVersion& server_version);
}

/**
 * @brief Exception classes for communication
 */
class CommunicationException : public std::exception {
public:
    explicit CommunicationException(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }

private:
    std::string message_;
};

class NetworkException : public CommunicationException {
public:
    explicit NetworkException(const std::string& message)
        : CommunicationException("Network error: " + message) {}
};

class ProtocolException : public CommunicationException {
public:
    explicit ProtocolException(const std::string& message)
        : CommunicationException("Protocol error: " + message) {}
};

class TimeoutException : public CommunicationException {
public:
    explicit TimeoutException(const std::string& operation)
        : CommunicationException("Timeout during " + operation) {}
};

} // namespace CodeCompiler

#endif // COMMUNICATION_H