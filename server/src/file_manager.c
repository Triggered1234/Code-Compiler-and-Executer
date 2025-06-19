/**
 * @file file_manager.c
 * @brief File management service for Code Compiler & Executer
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
#include <errno.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <libgen.h>

/* File manager constants */
#define MAX_WATCH_DESCRIPTORS 1000
#define INOTIFY_BUFFER_SIZE 8192
#define FILE_CLEANUP_INTERVAL 3600  /* 1 hour */
#define MAX_FILE_AGE 86400          /* 24 hours */

/* File tracking structure */
typedef struct file_entry {
    uint32_t job_id;
    uint32_t client_id;
    char filename[MAX_FILENAME_SIZE];
    char full_path[MAX_PATH_LEN];
    size_t file_size;
    time_t creation_time;
    time_t last_access;
    bool is_temporary;
    struct file_entry* next;
} file_entry_t;

/* Global file manager state */
static struct {
    file_entry_t* files_head;
    pthread_mutex_t files_mutex;
    int inotify_fd;
    int watch_descriptors[MAX_WATCH_DESCRIPTORS];
    size_t watch_count;
    pthread_t cleanup_thread;
    bool initialized;
} g_file_manager = {0};

/* Function prototypes */
static int setup_inotify_watches(void);
static void* file_cleanup_thread(void* arg);
static void* inotify_monitor_thread(void* arg);
static int add_file_entry(uint32_t job_id, uint32_t client_id, const char* filename, const char* full_path, size_t size, bool temporary);
static file_entry_t* find_file_entry(const char* filename);
static void remove_file_entry(const char* filename);
static void cleanup_old_files(void);
static int ensure_directory_structure(void);
static char* generate_unique_filename(const char* base_dir, const char* prefix, const char* extension);
static int safe_copy_file(const char* src, const char* dest);
static int validate_file_path(const char* path);

/**
 * @brief Initialize file manager
 */
int file_manager_init(void) {
    LOG_INFO("Initializing file manager");
    
    if (g_file_manager.initialized) {
        LOG_WARNING("File manager already initialized");
        return 0;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_file_manager.files_mutex, NULL) != 0) {
        LOG_ERROR("Failed to initialize file manager mutex: %s", strerror(errno));
        return -1;
    }
    
    // Ensure directory structure exists
    if (ensure_directory_structure() != 0) {
        LOG_ERROR("Failed to create directory structure");
        return -1;
    }
    
    // Setup inotify for file monitoring
    g_file_manager.inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (g_file_manager.inotify_fd == -1) {
        LOG_WARNING("Failed to initialize inotify: %s", strerror(errno));
        g_file_manager.inotify_fd = -1;
    } else {
        if (setup_inotify_watches() != 0) {
            LOG_WARNING("Failed to setup inotify watches");
        }
    }
    
    // Start cleanup thread
    if (pthread_create(&g_file_manager.cleanup_thread, NULL, file_cleanup_thread, NULL) != 0) {
        LOG_ERROR("Failed to create file cleanup thread: %s", strerror(errno));
        close(g_file_manager.inotify_fd);
        return -1;
    }
    
    g_file_manager.initialized = true;
    LOG_INFO("File manager initialized successfully");
    
    return 0;
}

/**
 * @brief Cleanup file manager
 */
void file_manager_cleanup(void) {
    if (!g_file_manager.initialized) {
        return;
    }
    
    LOG_INFO("Cleaning up file manager");
    
    g_file_manager.initialized = false;
    
    // Cancel cleanup thread
    pthread_cancel(g_file_manager.cleanup_thread);
    pthread_join(g_file_manager.cleanup_thread, NULL);
    
    // Close inotify
    if (g_file_manager.inotify_fd != -1) {
        close(g_file_manager.inotify_fd);
    }
    
    // Clean up file entries
    pthread_mutex_lock(&g_file_manager.files_mutex);
    
    file_entry_t* entry = g_file_manager.files_head;
    while (entry) {
        file_entry_t* next = entry->next;
        free(entry);
        entry = next;
    }
    g_file_manager.files_head = NULL;
    
    pthread_mutex_unlock(&g_file_manager.files_mutex);
    
    // Destroy mutex
    pthread_mutex_destroy(&g_file_manager.files_mutex);
    
    LOG_INFO("File manager cleanup completed");
}

/**
 * @brief Save uploaded file to storage
 */
int save_uploaded_file(uint32_t job_id, uint32_t client_id, const char* filename, 
                      const void* data, size_t size) {
    if (!filename || !data || size == 0) {
        LOG_ERROR("Invalid parameters for save_uploaded_file");
        return -1;
    }
    
    if (!g_file_manager.initialized) {
        LOG_ERROR("File manager not initialized");
        return -1;
    }
    
    // Validate filename
    if (!is_valid_filename(filename)) {
        LOG_ERROR("Invalid filename: %s", filename);
        return -1;
    }
    
    // Generate full path
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s/job_%u_%s", 
            g_server.config.processing_dir, job_id, filename);
    
    // Validate path
    if (validate_file_path(full_path) != 0) {
        LOG_ERROR("Invalid file path: %s", full_path);
        return -1;
    }
    
    // Check file size limits
    if (size > g_server.config.max_file_size) {
        LOG_ERROR("File too large: %zu bytes (max: %zu)", size, g_server.config.max_file_size);
        return -1;
    }
    
    // Create file
    int fd = open(full_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd == -1) {
        LOG_ERROR("Failed to create file %s: %s", full_path, strerror(errno));
        return -1;
    }
    
    // Write data
    ssize_t written = write(fd, data, size);
    if (written != (ssize_t)size) {
        LOG_ERROR("Failed to write complete file %s: %s", full_path, strerror(errno));
        close(fd);
        unlink(full_path);
        return -1;
    }
    
    close(fd);
    
    // Add to file tracking
    if (add_file_entry(job_id, client_id, filename, full_path, size, false) != 0) {
        LOG_WARNING("Failed to add file entry for tracking");
    }
    
    LOG_INFO("Saved uploaded file: %s (%zu bytes) for job %u", filename, size, job_id);
    
    return 0;
}

/**
 * @brief Load file from storage
 */
int load_file_content(const char* filename, void** data, size_t* size) {
    if (!filename || !data || !size) {
        LOG_ERROR("Invalid parameters for load_file_content");
        return -1;
    }
    
    if (!g_file_manager.initialized) {
        LOG_ERROR("File manager not initialized");
        return -1;
    }
    
    // Find file entry
    pthread_mutex_lock(&g_file_manager.files_mutex);
    file_entry_t* entry = find_file_entry(filename);
    if (!entry) {
        pthread_mutex_unlock(&g_file_manager.files_mutex);
        LOG_ERROR("File not found: %s", filename);
        return -1;
    }
    
    char full_path[MAX_PATH_LEN];
    strncpy(full_path, entry->full_path, sizeof(full_path) - 1);
    entry->last_access = time(NULL);
    
    pthread_mutex_unlock(&g_file_manager.files_mutex);
    
    // Open file
    int fd = open(full_path, O_RDONLY);
    if (fd == -1) {
        LOG_ERROR("Failed to open file %s: %s", full_path, strerror(errno));
        return -1;
    }
    
    // Get file size
    struct stat st;
    if (fstat(fd, &st) == -1) {
        LOG_ERROR("Failed to get file stats %s: %s", full_path, strerror(errno));
        close(fd);
        return -1;
    }
    
    if (st.st_size > (off_t)g_server.config.max_file_size) {
        LOG_ERROR("File too large: %s", full_path);
        close(fd);
        return -1;
    }
    
    // Allocate buffer
    *data = malloc(st.st_size);
    if (!*data) {
        LOG_ERROR("Failed to allocate memory for file content");
        close(fd);
        return -1;
    }
    
    // Read file
    ssize_t bytes_read = read(fd, *data, st.st_size);
    if (bytes_read != st.st_size) {
        LOG_ERROR("Failed to read complete file %s: %s", full_path, strerror(errno));
        close(fd);
        free(*data);
        *data = NULL;
        return -1;
    }
    
    close(fd);
    *size = st.st_size;
    
    LOG_DEBUG("Loaded file content: %s (%zu bytes)", filename, *size);
    
    return 0;
}

/**
 * @brief Delete file and cleanup
 */
int delete_file(uint32_t job_id, const char* filename) {
    if (!filename) {
        LOG_ERROR("Invalid filename for delete_file");
        return -1;
    }
    
    if (!g_file_manager.initialized) {
        LOG_ERROR("File manager not initialized");
        return -1;
    }
    
    // Find file entry
    pthread_mutex_lock(&g_file_manager.files_mutex);
    file_entry_t* entry = find_file_entry(filename);
    if (!entry || entry->job_id != job_id) {
        pthread_mutex_unlock(&g_file_manager.files_mutex);
        LOG_ERROR("File not found or access denied: %s", filename);
        return -1;
    }
    
    char full_path[MAX_PATH_LEN];
    strncpy(full_path, entry->full_path, sizeof(full_path) - 1);
    
    // Remove from tracking
    remove_file_entry(filename);
    
    pthread_mutex_unlock(&g_file_manager.files_mutex);
    
    // Delete physical file
    if (unlink(full_path) == -1) {
        LOG_ERROR("Failed to delete file %s: %s", full_path, strerror(errno));
        return -1;
    }
    
    LOG_INFO("Deleted file: %s", filename);
    
    return 0;
}

/**
 * @brief Create temporary file for job
 */
char* create_temp_file(uint32_t job_id, const char* extension) {
    if (!extension) extension = "tmp";
    
    if (!g_file_manager.initialized) {
        LOG_ERROR("File manager not initialized");
        return NULL;
    }
    
    // Generate unique filename
    char* filename = generate_unique_filename(g_server.config.processing_dir, "temp", extension);
    if (!filename) {
        LOG_ERROR("Failed to generate unique filename");
        return NULL;
    }
    
    // Create empty file
    int fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd == -1) {
        LOG_ERROR("Failed to create temp file %s: %s", filename, strerror(errno));
        free(filename);
        return NULL;
    }
    
    close(fd);
    
    // Add to tracking as temporary file
    char* basename_str = basename(filename);
    if (add_file_entry(job_id, 0, basename_str, filename, 0, true) != 0) {
        LOG_WARNING("Failed to add temp file entry for tracking");
    }
    
    LOG_DEBUG("Created temporary file: %s", filename);
    
    return filename;
}

/**
 * @brief Get file information
 */
int get_file_info(const char* filename, file_info_t* info) {
    if (!filename || !info) {
        LOG_ERROR("Invalid parameters for get_file_info");
        return -1;
    }
    
    if (!g_file_manager.initialized) {
        LOG_ERROR("File manager not initialized");
        return -1;
    }
    
    pthread_mutex_lock(&g_file_manager.files_mutex);
    
    file_entry_t* entry = find_file_entry(filename);
    if (!entry) {
        pthread_mutex_unlock(&g_file_manager.files_mutex);
        LOG_ERROR("File not found: %s", filename);
        return -1;
    }
    
    // Fill info structure
    memset(info, 0, sizeof(file_info_t));
    info->job_id = entry->job_id;
    info->client_id = entry->client_id;
    strncpy(info->filename, entry->filename, sizeof(info->filename) - 1);
    info->file_size = entry->file_size;
    info->creation_time = entry->creation_time;
    info->last_access = entry->last_access;
    info->is_temporary = entry->is_temporary;
    
    // Get additional file stats
    struct stat st;
    if (stat(entry->full_path, &st) == 0) {
        info->file_size = st.st_size;  // Update with current size
        info->last_modified = st.st_mtime;
        info->permissions = st.st_mode;
    }
    
    pthread_mutex_unlock(&g_file_manager.files_mutex);
    
    return 0;
}

/**
 * @brief List files for a job
 */
int list_job_files(uint32_t job_id, file_info_t** files, size_t* count) {
    if (!files || !count) {
        LOG_ERROR("Invalid parameters for list_job_files");
        return -1;
    }
    
    if (!g_file_manager.initialized) {
        LOG_ERROR("File manager not initialized");
        return -1;
    }
    
    pthread_mutex_lock(&g_file_manager.files_mutex);
    
    // Count matching files
    size_t file_count = 0;
    file_entry_t* entry = g_file_manager.files_head;
    while (entry) {
        if (entry->job_id == job_id) {
            file_count++;
        }
        entry = entry->next;
    }
    
    if (file_count == 0) {
        *files = NULL;
        *count = 0;
        pthread_mutex_unlock(&g_file_manager.files_mutex);
        return 0;
    }
    
    // Allocate array
    *files = calloc(file_count, sizeof(file_info_t));
    if (!*files) {
        pthread_mutex_unlock(&g_file_manager.files_mutex);
        LOG_ERROR("Failed to allocate memory for file list");
        return -1;
    }
    
    // Fill array
    size_t index = 0;
    entry = g_file_manager.files_head;
    while (entry && index < file_count) {
        if (entry->job_id == job_id) {
            file_info_t* info = &(*files)[index];
            info->job_id = entry->job_id;
            info->client_id = entry->client_id;
            strncpy(info->filename, entry->filename, sizeof(info->filename) - 1);
            info->file_size = entry->file_size;
            info->creation_time = entry->creation_time;
            info->last_access = entry->last_access;
            info->is_temporary = entry->is_temporary;
            
            // Get current file stats
            struct stat st;
            if (stat(entry->full_path, &st) == 0) {
                info->file_size = st.st_size;
                info->last_modified = st.st_mtime;
                info->permissions = st.st_mode;
            }
            
            index++;
        }
        entry = entry->next;
    }
    
    *count = file_count;
    
    pthread_mutex_unlock(&g_file_manager.files_mutex);
    
    LOG_DEBUG("Listed %zu files for job %u", file_count, job_id);
    
    return 0;
}

/**
 * @brief Cleanup files for a completed job
 */
int cleanup_job_files(uint32_t job_id) {
    if (!g_file_manager.initialized) {
        LOG_ERROR("File manager not initialized");
        return -1;
    }
    
    pthread_mutex_lock(&g_file_manager.files_mutex);
    
    file_entry_t* entry = g_file_manager.files_head;
    file_entry_t* prev = NULL;
    int files_deleted = 0;
    
    while (entry) {
        file_entry_t* next = entry->next;
        
        if (entry->job_id == job_id) {
            // Delete physical file
            if (unlink(entry->full_path) == 0) {
                files_deleted++;
                LOG_DEBUG("Deleted file: %s", entry->full_path);
            } else {
                LOG_WARNING("Failed to delete file %s: %s", entry->full_path, strerror(errno));
            }
            
            // Remove from list
            if (prev) {
                prev->next = next;
            } else {
                g_file_manager.files_head = next;
            }
            
            free(entry);
        } else {
            prev = entry;
        }
        
        entry = next;
    }
    
    pthread_mutex_unlock(&g_file_manager.files_mutex);
    
    if (files_deleted > 0) {
        LOG_INFO("Cleaned up %d files for job %u", files_deleted, job_id);
    }
    
    return files_deleted;
}

/**
 * @brief Setup inotify watches for file monitoring
 */
static int setup_inotify_watches(void) {
    if (g_file_manager.inotify_fd == -1) {
        return -1;
    }
    
    // Watch processing directory
    int wd = inotify_add_watch(g_file_manager.inotify_fd, 
                              g_server.config.processing_dir,
                              IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_TO | IN_MOVED_FROM);
    if (wd == -1) {
        LOG_ERROR("Failed to add inotify watch for %s: %s", 
                 g_server.config.processing_dir, strerror(errno));
        return -1;
    }
    
    g_file_manager.watch_descriptors[g_file_manager.watch_count++] = wd;
    
    // Watch outgoing directory
    wd = inotify_add_watch(g_file_manager.inotify_fd,
                          g_server.config.outgoing_dir,
                          IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_TO | IN_MOVED_FROM);
    if (wd == -1) {
        LOG_ERROR("Failed to add inotify watch for %s: %s",
                 g_server.config.outgoing_dir, strerror(errno));
        return -1;
    }
    
    g_file_manager.watch_descriptors[g_file_manager.watch_count++] = wd;
    
    LOG_DEBUG("Setup inotify watches for file monitoring");
    
    return 0;
}

/**
 * @brief File cleanup thread
 */
static void* file_cleanup_thread(void* arg) {
    UNUSED(arg);
    
    LOG_INFO("File cleanup thread started");
    
    while (g_file_manager.initialized) {
        // Sleep for cleanup interval
        sleep(FILE_CLEANUP_INTERVAL);
        
        if (!g_file_manager.initialized) break;
        
        // Cleanup old files
        cleanup_old_files();
    }
    
    LOG_INFO("File cleanup thread stopped");
    return NULL;
}

/**
 * @brief Cleanup old files
 */
static void cleanup_old_files(void) {
    time_t now = time(NULL);
    int files_cleaned = 0;
    
    pthread_mutex_lock(&g_file_manager.files_mutex);
    
    file_entry_t* entry = g_file_manager.files_head;
    file_entry_t* prev = NULL;
    
    while (entry) {
        file_entry_t* next = entry->next;
        
        // Check if file is old and temporary
        if (entry->is_temporary && (now - entry->creation_time) > MAX_FILE_AGE) {
            // Delete physical file
            if (unlink(entry->full_path) == 0) {
                files_cleaned++;
                LOG_DEBUG("Cleaned up old temporary file: %s", entry->full_path);
            } else {
                LOG_WARNING("Failed to cleanup file %s: %s", entry->full_path, strerror(errno));
            }
            
            // Remove from list
            if (prev) {
                prev->next = next;
            } else {
                g_file_manager.files_head = next;
            }
            
            free(entry);
        } else {
            prev = entry;
        }
        
        entry = next;
    }
    
    pthread_mutex_unlock(&g_file_manager.files_mutex);
    
    if (files_cleaned > 0) {
        LOG_INFO("Cleaned up %d old files", files_cleaned);
    }
}

/**
 * @brief Add file entry to tracking list
 */
static int add_file_entry(uint32_t job_id, uint32_t client_id, const char* filename, 
                         const char* full_path, size_t size, bool temporary) {
    file_entry_t* entry = calloc(1, sizeof(file_entry_t));
    if (!entry) {
        LOG_ERROR("Failed to allocate file entry");
        return -1;
    }
    
    entry->job_id = job_id;
    entry->client_id = client_id;
    strncpy(entry->filename, filename, sizeof(entry->filename) - 1);
    strncpy(entry->full_path, full_path, sizeof(entry->full_path) - 1);
    entry->file_size = size;
    entry->creation_time = time(NULL);
    entry->last_access = entry->creation_time;
    entry->is_temporary = temporary;
    
    pthread_mutex_lock(&g_file_manager.files_mutex);
    
    // Add to head of list
    entry->next = g_file_manager.files_head;
    g_file_manager.files_head = entry;
    
    pthread_mutex_unlock(&g_file_manager.files_mutex);
    
    return 0;
}

/**
 * @brief Find file entry by filename
 */
static file_entry_t* find_file_entry(const char* filename) {
    file_entry_t* entry = g_file_manager.files_head;
    
    while (entry) {
        if (strcmp(entry->filename, filename) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

/**
 * @brief Remove file entry from tracking list
 */
static void remove_file_entry(const char* filename) {
    file_entry_t* entry = g_file_manager.files_head;
    file_entry_t* prev = NULL;
    
    while (entry) {
        if (strcmp(entry->filename, filename) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                g_file_manager.files_head = entry->next;
            }
            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

/**
 * @brief Ensure directory structure exists
 */
static int ensure_directory_structure(void) {
    const char* dirs[] = {
        g_server.config.processing_dir,
        g_server.config.outgoing_dir,
        NULL
    };
    
    for (int i = 0; dirs[i]; i++) {
        if (mkdir(dirs[i], 0755) == -1 && errno != EEXIST) {
            LOG_ERROR("Failed to create directory %s: %s", dirs[i], strerror(errno));
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief Generate unique filename
 */
static char* generate_unique_filename(const char* base_dir, const char* prefix, const char* extension) {
    char* filename = malloc(MAX_PATH_LEN);
    if (!filename) return NULL;
    
    time_t now = time(NULL);
    pid_t pid = getpid();
    
    for (int attempt = 0; attempt < 1000; attempt++) {
        snprintf(filename, MAX_PATH_LEN, "%s/%s_%ld_%d_%d.%s",
                base_dir, prefix, (long)now, (int)pid, attempt, extension);
        
        // Check if file exists
        if (access(filename, F_OK) == -1) {
            return filename; // File doesn't exist, we can use this name
        }
    }
    
    free(filename);
    return NULL; // Failed to generate unique name
}

/**
 * @brief Validate file path for security
 */
static int validate_file_path(const char* path) {
    if (!path) return -1;
    
    // Check for directory traversal
    if (strstr(path, "..") || strstr(path, "//")) {
        LOG_ERROR("Invalid file path contains directory traversal: %s", path);
        return -1;
    }
    
    // Check if path is within allowed directories
    if (strncmp(path, g_server.config.processing_dir, strlen(g_server.config.processing_dir)) != 0 &&
        strncmp(path, g_server.config.outgoing_dir, strlen(g_server.config.outgoing_dir)) != 0) {
        LOG_ERROR("File path outside allowed directories: %s", path);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Check if filename is valid
 */
bool is_valid_filename(const char* filename) {
    if (!filename || strlen(filename) == 0) return false;
    
    // Check length
    if (strlen(filename) > MAX_FILENAME_SIZE) return false;
    
    // Check for invalid characters
    const char* invalid_chars = "/<>:\"|?*\\";
    for (const char* p = filename; *p; p++) {
        if (strchr(invalid_chars, *p)) return false;
        if (*p < 32) return false; // Control characters
    }
    
    // Check for reserved names on Windows (for compatibility)
    const char* reserved[] = {"CON", "PRN", "AUX", "NUL", 
                             "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                             "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
                             NULL};
    
    for (int i = 0; reserved[i]; i++) {
        if (strcasecmp(filename, reserved[i]) == 0) return false;
    }
    
    return true;
}

/**
 * @brief Get file manager statistics
 */
int get_file_manager_stats(file_manager_stats_t* stats) {
    if (!stats) return -1;
    
    if (!g_file_manager.initialized) {
        memset(stats, 0, sizeof(file_manager_stats_t));
        return 0;
    }
    
    pthread_mutex_lock(&g_file_manager.files_mutex);
    
    memset(stats, 0, sizeof(file_manager_stats_t));
    
    file_entry_t* entry = g_file_manager.files_head;
    while (entry) {
        stats->total_files++;
        stats->total_size += entry->file_size;
        
        if (entry->is_temporary) {
            stats->temporary_files++;
        }
        
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&g_file_manager.files_mutex);
    
    return 0;
}