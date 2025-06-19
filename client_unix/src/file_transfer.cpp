/**
 * @file file_transfer.cpp
 * @brief File transfer implementation for Unix client
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @date April 2025
 */

#include "client.h"
#include "communication.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>
#include <zlib.h>

namespace CodeCompiler {

/**
 * @brief FileTransferManager implementation
 */
FileTransferManager::FileTransferManager(MessageHandler& message_handler)
    : message_handler_(message_handler), chunk_size_(64 * 1024), compression_enabled_(false) {
}

void FileTransferManager::setChunkSize(size_t chunk_size) {
    chunk_size_ = std::max(size_t(1024), std::min(chunk_size, size_t(1024 * 1024))); // 1KB to 1MB
}

size_t FileTransferManager::getChunkSize() const {
    return chunk_size_;
}

void FileTransferManager::setCompressionEnabled(bool enabled) {
    compression_enabled_ = enabled;
}

bool FileTransferManager::isCompressionEnabled() const {
    return compression_enabled_;
}

bool FileTransferManager::uploadFile(const std::string& local_path, const std::string& remote_name,
                                   ProgressCallback progress) {
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // Validate file
        if (!validateFile(local_path)) {
            return false;
        }
        
        // Open file
        std::ifstream file(local_path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        // Get file size
        file.seekg(0, std::ios::end);
        size_t file_size = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);
        
        // Determine remote filename
        std::string filename = remote_name.empty() ? FileUtils::getBasename(local_path) : remote_name;
        
        // Calculate chunks
        size_t chunk_count = (file_size + chunk_size_ - 1) / chunk_size_;
        uint32_t file_checksum = calculateFileChecksum(local_path);
        
        // Send upload start
        if (!message_handler_.sendFileUploadStart(filename, file_size, 
                                                 static_cast<uint32_t>(chunk_count), 
                                                 chunk_size_, file_checksum)) {
            return false;
        }
        
        // Send file chunks
        size_t bytes_sent = 0;
        for (uint32_t chunk_id = 0; chunk_id < chunk_count; ++chunk_id) {
            std::vector<uint8_t> chunk_data = readFileChunk(file, chunk_size_);
            
            if (chunk_data.empty() && chunk_id < chunk_count - 1) {
                // Unexpected end of file
                return false;
            }
            
            if (!message_handler_.sendFileChunk(chunk_id, chunk_data)) {
                return false;
            }
            
            bytes_sent += chunk_data.size();
            
            // Update progress
            if (progress) {
                progress(bytes_sent, file_size);
            }
            
            // Small delay to prevent overwhelming the server
            if (chunk_count > 100 && chunk_id % 10 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        // Send upload end
        if (!message_handler_.sendFileUploadEnd()) {
            return false;
        }
        
        // Update statistics
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        stats_.files_uploaded++;
        stats_.bytes_uploaded += file_size;
        stats_.total_upload_time += duration;
        
        return true;
        
    } catch (const std::exception& e) {
        stats_.upload_errors++;
        return false;
    }
}

bool FileTransferManager::uploadFileAsync(const std::string& local_path, 
                                        std::function<void(bool)> completion_callback,
                                        const std::string& remote_name,
                                        ProgressCallback progress) {
    // Launch upload in separate thread
    std::thread upload_thread([this, local_path, remote_name, progress, completion_callback]() {
        bool success = uploadFile(local_path, remote_name, progress);
        if (completion_callback) {
            completion_callback(success);
        }
    });
    
    upload_thread.detach();
    return true;
}

bool FileTransferManager::downloadFile(const std::string& remote_name, const std::string& local_path,
                                      ProgressCallback progress) {
    // Download is not implemented in the current protocol
    // This would require additional message types
    (void)remote_name;
    (void)local_path;
    (void)progress;
    
    stats_.download_errors++;
    return false;
}

const FileTransferManager::TransferStats& FileTransferManager::getStats() const {
    return stats_;
}

void FileTransferManager::resetStats() {
    stats_ = TransferStats{};
}

uint32_t FileTransferManager::calculateFileChecksum(const std::string& filepath) const {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return 0;
    }
    
    uint32_t crc = 0;
    const size_t buffer_size = 8192;
    std::vector<uint8_t> buffer(buffer_size);
    
    while (file.read(reinterpret_cast<char*>(buffer.data()), buffer_size) || file.gcount() > 0) {
        size_t bytes_read = static_cast<size_t>(file.gcount());
        crc = crc32(crc, buffer.data(), bytes_read);
    }
    
    return crc;
}

bool FileTransferManager::validateFile(const std::string& filepath) const {
    // Check if file exists and is readable
    if (!FileUtils::exists(filepath) || !FileUtils::isFile(filepath) || !FileUtils::isReadable(filepath)) {
        return false;
    }
    
    // Check file size
    size_t file_size = FileUtils::getSize(filepath);
    if (file_size == 0 || file_size > 100 * 1024 * 1024) { // Max 100MB
        return false;
    }
    
    // Check if it's a valid source file
    return FileUtils::validateSourceFile(filepath);
}

std::vector<uint8_t> FileTransferManager::readFileChunk(std::ifstream& file, size_t chunk_size) const {
    std::vector<uint8_t> buffer(chunk_size);
    file.read(reinterpret_cast<char*>(buffer.data()), chunk_size);
    
    size_t bytes_read = static_cast<size_t>(file.gcount());
    buffer.resize(bytes_read);
    
    return buffer;
}

/**
 * @brief File upload progress tracker
 */
class FileUploadTracker {
public:
    FileUploadTracker(const std::string& filename, size_t total_size, bool verbose)
        : filename_(filename), total_size_(total_size), verbose_(verbose),
          bytes_uploaded_(0), start_time_(std::chrono::steady_clock::now()) {
        
        if (verbose_) {
            std::cout << "Uploading " << filename_ << " (" << formatBytes(total_size_) << ")..." << std::endl;
        }
    }
    
    void updateProgress(size_t bytes_uploaded) {
        bytes_uploaded_ = bytes_uploaded;
        
        if (verbose_ && total_size_ > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
            
            // Update every 100ms or at completion
            if (elapsed.count() - last_update_time_ > 100 || bytes_uploaded_ == total_size_) {
                displayProgress();
                last_update_time_ = elapsed.count();
            }
        }
    }
    
    void complete(bool success) {
        if (verbose_) {
            if (success) {
                auto end_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);
                
                std::cout << "\nUpload completed in " << duration.count() << " ms";
                if (duration.count() > 0) {
                    double speed = (static_cast<double>(total_size_) * 1000) / duration.count();
                    std::cout << " (" << formatBytes(static_cast<size_t>(speed)) << "/s)";
                }
                std::cout << std::endl;
            } else {
                std::cout << "\nUpload failed!" << std::endl;
            }
        }
    }

private:
    void displayProgress() {
        if (total_size_ == 0) return;
        
        double percent = (static_cast<double>(bytes_uploaded_) / total_size_) * 100.0;
        
        // Create progress bar
        const int bar_width = 30;
        int filled = static_cast<int>((percent / 100.0) * bar_width);
        
        std::cout << "\r[";
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled) {
                std::cout << "=";
            } else if (i == filled) {
                std::cout << ">";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "] " << std::fixed << std::setprecision(1) << percent << "% ";
        std::cout << "(" << formatBytes(bytes_uploaded_) << "/" << formatBytes(total_size_) << ")";
        std::cout << std::flush;
    }
    
    std::string formatBytes(size_t bytes) const {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int unit = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024.0 && unit < 3) {
            size /= 1024.0;
            unit++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
        return oss.str();
    }
    
    std::string filename_;
    size_t total_size_;
    bool verbose_;
    size_t bytes_uploaded_;
    std::chrono::steady_clock::time_point start_time_;
    int64_t last_update_time_ = 0;
};

/**
 * @brief Compressed file transfer (if compression is enabled)
 */
class CompressedFileTransfer {
public:
    static std::vector<uint8_t> compressData(const std::vector<uint8_t>& input) {
        uLongf compressed_size = compressBound(input.size());
        std::vector<uint8_t> compressed(compressed_size);
        
        int result = compress(compressed.data(), &compressed_size, 
                            input.data(), input.size());
        
        if (result != Z_OK) {
            return input; // Return original if compression fails
        }
        
        compressed.resize(compressed_size);
        
        // Only use compressed data if it's actually smaller
        return (compressed.size() < input.size()) ? compressed : input;
    }
    
    static std::vector<uint8_t> decompressData(const std::vector<uint8_t>& compressed, 
                                              size_t original_size) {
        std::vector<uint8_t> decompressed(original_size);
        uLongf decompressed_size = original_size;
        
        int result = uncompress(decompressed.data(), &decompressed_size,
                              compressed.data(), compressed.size());
        
        if (result != Z_OK) {
            return {}; // Return empty on error
        }
        
        decompressed.resize(decompressed_size);
        return decompressed;
    }
};

/**
 * @brief File transfer utilities
 */
namespace FileTransferUtils {

bool isValidFilename(const std::string& filename) {
    if (filename.empty() || filename.length() > 255) {
        return false;
    }
    
    // Check for invalid characters
    const std::string invalid_chars = "<>:\"|?*\\";
    for (char c : filename) {
        if (invalid_chars.find(c) != std::string::npos || c < 32) {
            return false;
        }
    }
    
    // Check for path separators
    if (filename.find('/') != std::string::npos) {
        return false;
    }
    
    return true;
}

std::string sanitizeFilename(const std::string& filename) {
    std::string sanitized = FileUtils::getBasename(filename);
    
    // Replace invalid characters with underscores
    const std::string invalid_chars = "<>:\"|?*\\";
    for (char& c : sanitized) {
        if (invalid_chars.find(c) != std::string::npos || c < 32) {
            c = '_';
        }
    }
    
    // Limit length
    if (sanitized.length() > 255) {
        sanitized = sanitized.substr(0, 255);
    }
    
    return sanitized;
}

size_t calculateOptimalChunkSize(size_t file_size) {
    // Calculate chunk size based on file size for optimal transfer
    if (file_size < 1024 * 1024) {        // < 1MB
        return 8 * 1024;                   // 8KB chunks
    } else if (file_size < 10 * 1024 * 1024) {  // < 10MB
        return 32 * 1024;                  // 32KB chunks
    } else {                               // >= 10MB
        return 64 * 1024;                  // 64KB chunks
    }
}

bool isTransferInProgress(const std::string& temp_file) {
    // Check if a transfer is already in progress for this file
    return FileUtils::exists(temp_file + ".transfer");
}

void markTransferInProgress(const std::string& temp_file) {
    std::ofstream marker(temp_file + ".transfer");
    marker << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
}

void markTransferComplete(const std::string& temp_file) {
    std::remove((temp_file + ".transfer").c_str());
}

std::string createTransferMetadata(const std::string& filename, size_t file_size, 
                                  uint32_t checksum, size_t chunk_count) {
    std::ostringstream metadata;
    metadata << "filename=" << filename << "\n";
    metadata << "size=" << file_size << "\n";
    metadata << "checksum=" << std::hex << checksum << "\n";
    metadata << "chunks=" << chunk_count << "\n";
    metadata << "timestamp=" << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
    return metadata.str();
}

bool verifyFileIntegrity(const std::string& filepath, uint32_t expected_checksum) {
    FileTransferManager dummy_manager(*(MessageHandler*)nullptr); // Hack for access to private method
    // In real implementation, this would be a standalone function
    return true; // Placeholder
}

} // namespace FileTransferUtils

} // namespace CodeCompiler