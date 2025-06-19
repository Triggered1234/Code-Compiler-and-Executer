"""
File transfer module for Code Compiler & Executer Python Client
Handles file upload and download operations
Authors: Rares-Nicholas Popa & Adrian-Petru Enache
Date: April 2025
"""

import os
import struct
import hashlib
import math
from typing import Optional, Callable
from communication import Message, MessageType, MessageHandler


class FileTransferError(Exception):
    """File transfer related errors"""
    pass


class FileTransferManager:
    """Manages file upload and download operations"""
    
    def __init__(self, message_handler: MessageHandler, chunk_size: int = 64 * 1024):
        self.message_handler = message_handler
        self.chunk_size = chunk_size
        self.verbose = message_handler.verbose
    
    def upload_file(self, filepath: str, 
                   progress_callback: Optional[Callable[[int, int], None]] = None) -> bool:
        """
        Upload a file to the server
        
        Args:
            filepath: Path to file to upload
            progress_callback: Optional callback function(bytes_sent, total_bytes)
        
        Returns:
            True if upload successful, False otherwise
        """
        if not os.path.exists(filepath):
            raise FileTransferError(f"File not found: {filepath}")
        
        if not os.path.isfile(filepath):
            raise FileTransferError(f"Path is not a file: {filepath}")
        
        try:
            file_size = os.path.getsize(filepath)
            filename = os.path.basename(filepath)
            
            if self.verbose:
                print(f"Uploading {filename} ({file_size} bytes)...")
            
            # Step 1: Send upload start message
            if not self._send_upload_start(filename, file_size):
                return False
            
            # Step 2: Send file chunks
            chunk_count = math.ceil(file_size / self.chunk_size)
            bytes_sent = 0
            
            with open(filepath, 'rb') as f:
                for chunk_id in range(chunk_count):
                    chunk_data = f.read(self.chunk_size)
                    if not chunk_data:
                        break
                    
                    if not self._send_file_chunk(chunk_id, chunk_data):
                        return False
                    
                    bytes_sent += len(chunk_data)
                    
                    # Call progress callback
                    if progress_callback:
                        progress_callback(bytes_sent, file_size)
                    
                    if self.verbose and chunk_id % 10 == 0:  # Show progress every 10 chunks
                        percent = (bytes_sent / file_size) * 100
                        print(f"Upload progress: {percent:.1f}% ({bytes_sent}/{file_size})")
            
            # Step 3: Send upload end message
            if not self._send_upload_end():
                return False
            
            if self.verbose:
                print(f"Upload completed: {filename}")
            
            return True
            
        except Exception as e:
            if self.verbose:
                print(f"Upload failed: {e}")
            return False
    
    def _send_upload_start(self, filename: str, file_size: int) -> bool:
        """Send file upload start message"""
        try:
            # Calculate file checksum
            file_checksum = self._calculate_file_checksum_from_path(filename)
            chunk_count = math.ceil(file_size / self.chunk_size)
            
            # Pack upload start data
            filename_bytes = filename.encode('utf-8')[:256]
            data = struct.pack('>Q I I 256s I',
                             file_size,
                             chunk_count,
                             self.chunk_size,
                             filename_bytes,
                             file_checksum)
            
            msg = Message(MessageType.MSG_FILE_UPLOAD_START, data, 
                         self.message_handler._next_correlation_id())
            
            response = self.message_handler.send_and_receive(msg)
            
            if response and response.message_type == MessageType.MSG_ACK:
                return True
            elif response and response.message_type == MessageType.MSG_ERROR:
                error_info = self.message_handler._parse_error_response(response)
                print(f"Upload start failed: {error_info.get('error_message', 'Unknown error')}")
                return False
            
            return False
            
        except Exception as e:
            if self.verbose:
                print(f"Send upload start failed: {e}")
            return False
    
    def _send_file_chunk(self, chunk_id: int, chunk_data: bytes) -> bool:
        """Send a file chunk"""
        try:
            # Calculate chunk checksum
            import zlib
            chunk_checksum = zlib.crc32(chunk_data) & 0xffffffff
            
            # Pack chunk header
            chunk_header = struct.pack('>I I I',
                                     chunk_id,
                                     len(chunk_data),
                                     chunk_checksum)
            
            # Combine header and data
            data = chunk_header + chunk_data
            
            msg = Message(MessageType.MSG_FILE_UPLOAD_CHUNK, data,
                         self.message_handler._next_correlation_id())
            
            response = self.message_handler.send_and_receive(msg)
            
            if response and response.message_type == MessageType.MSG_ACK:
                return True
            elif response and response.message_type == MessageType.MSG_ERROR:
                error_info = self.message_handler._parse_error_response(response)
                print(f"Chunk {chunk_id} upload failed: {error_info.get('error_message', 'Unknown error')}")
                return False
            
            return False
            
        except Exception as e:
            if self.verbose:
                print(f"Send chunk {chunk_id} failed: {e}")
            return False
    
    def _send_upload_end(self) -> bool:
        """Send file upload end message"""
        try:
            msg = Message(MessageType.MSG_FILE_UPLOAD_END, b'',
                         self.message_handler._next_correlation_id())
            
            response = self.message_handler.send_and_receive(msg)
            
            if response and response.message_type == MessageType.MSG_ACK:
                return True
            elif response and response.message_type == MessageType.MSG_ERROR:
                error_info = self.message_handler._parse_error_response(response)
                print(f"Upload end failed: {error_info.get('error_message', 'Unknown error')}")
                return False
            
            return False
            
        except Exception as e:
            if self.verbose:
                print(f"Send upload end failed: {e}")
            return False
    
    def _calculate_file_checksum_from_path(self, filepath: str) -> int:
        """Calculate CRC32 checksum of a file"""
        import zlib
        checksum = 0
        
        try:
            with open(filepath, 'rb') as f:
                while True:
                    chunk = f.read(8192)
                    if not chunk:
                        break
                    checksum = zlib.crc32(chunk, checksum)
            
            return checksum & 0xffffffff
            
        except Exception:
            return 0
    
    def download_file(self, remote_filename: str, local_filepath: str,
                     progress_callback: Optional[Callable[[int, int], None]] = None) -> bool:
        """
        Download a file from the server (placeholder implementation)
        
        Args:
            remote_filename: Name of file on server
            local_filepath: Local path to save file
            progress_callback: Optional callback function(bytes_received, total_bytes)
        
        Returns:
            True if download successful, False otherwise
        """
        # Note: This is a placeholder implementation
        # The actual protocol would need additional message types for file download
        print(f"File download not yet implemented: {remote_filename} -> {local_filepath}")
        return False
    
    def verify_file_integrity(self, filepath: str, expected_checksum: int) -> bool:
        """Verify file integrity using checksum"""
        try:
            actual_checksum = self._calculate_file_checksum_from_path(filepath)
            return actual_checksum == expected_checksum
        except Exception:
            return False


class FileUploadProgress:
    """Helper class for tracking upload progress"""
    
    def __init__(self, total_size: int, verbose: bool = False):
        self.total_size = total_size
        self.bytes_uploaded = 0
        self.verbose = verbose
        self.start_time = None
        self.last_update_time = None
    
    def start(self):
        """Start progress tracking"""
        import time
        self.start_time = time.time()
        self.last_update_time = self.start_time
    
    def update(self, bytes_uploaded: int):
        """Update progress"""
        import time
        self.bytes_uploaded = bytes_uploaded
        current_time = time.time()
        
        if self.verbose and (current_time - self.last_update_time) >= 1.0:  # Update every second
            self._print_progress()
            self.last_update_time = current_time
    
    def finish(self):
        """Finish progress tracking"""
        if self.verbose:
            self._print_progress()
            print()  # New line
    
    def _print_progress(self):
        """Print current progress"""
        import time
        
        if self.total_size == 0:
            return
        
        percent = (self.bytes_uploaded / self.total_size) * 100
        
        # Calculate speed
        elapsed = time.time() - self.start_time if self.start_time else 0
        if elapsed > 0:
            speed = self.bytes_uploaded / elapsed
            speed_str = self._format_bytes(speed) + "/s"
        else:
            speed_str = "0 B/s"
        
        # Calculate ETA
        if elapsed > 0 and self.bytes_uploaded > 0:
            remaining_bytes = self.total_size - self.bytes_uploaded
            eta_seconds = (remaining_bytes / self.bytes_uploaded) * elapsed
            eta_str = self._format_time(eta_seconds)
        else:
            eta_str = "Unknown"
        
        print(f"\rProgress: {percent:5.1f}% "
              f"({self._format_bytes(self.bytes_uploaded)}/{self._format_bytes(self.total_size)}) "
              f"Speed: {speed_str} ETA: {eta_str}", end='', flush=True)
    
    def _format_bytes(self, bytes_value: float) -> str:
        """Format bytes in human readable format"""
        for unit in ['B', 'KB', 'MB', 'GB']:
            if bytes_value < 1024.0:
                return f"{bytes_value:.1f} {unit}"
            bytes_value /= 1024.0
        return f"{bytes_value:.1f} TB"
    
    def _format_time(self, seconds: float) -> str:
        """Format time in human readable format"""
        if seconds < 60:
            return f"{seconds:.0f}s"
        elif seconds < 3600:
            minutes = seconds / 60
            return f"{minutes:.0f}m"
        else:
            hours = seconds / 3600
            return f"{hours:.1f}h"


class ChunkedFileReader:
    """Helper class for reading files in chunks"""
    
    def __init__(self, filepath: str, chunk_size: int = 64 * 1024):
        self.filepath = filepath
        self.chunk_size = chunk_size
        self.file_size = os.path.getsize(filepath)
        self.chunks_total = math.ceil(self.file_size / chunk_size)
        self._file_handle = None
        self._current_chunk = 0
    
    def __enter__(self):
        self._file_handle = open(self.filepath, 'rb')
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        if self._file_handle:
            self._file_handle.close()
    
    def read_next_chunk(self) -> Optional[bytes]:
        """Read next chunk from file"""
        if not self._file_handle or self._current_chunk >= self.chunks_total:
            return None
        
        chunk_data = self._file_handle.read(self.chunk_size)
        self._current_chunk += 1
        
        return chunk_data if chunk_data else None
    
    def get_progress(self) -> tuple:
        """Get current progress (current_chunk, total_chunks)"""
        return (self._current_chunk, self.chunks_total)
    
    def calculate_checksum(self) -> int:
        """Calculate checksum of entire file"""
        import zlib
        checksum = 0
        
        if self._file_handle:
            current_pos = self._file_handle.tell()
            self._file_handle.seek(0)
            
            while True:
                chunk = self._file_handle.read(8192)
                if not chunk:
                    break
                checksum = zlib.crc32(chunk, checksum)
            
            self._file_handle.seek(current_pos)
        
        return checksum & 0xffffffff


# Utility functions
def is_binary_file(filepath: str) -> bool:
    """Check if file is binary"""
    try:
        with open(filepath, 'rb') as f:
            chunk = f.read(1024)
            return b'\x00' in chunk
    except Exception:
        return False


def get_file_mime_type(filepath: str) -> str:
    """Get MIME type of file based on extension"""
    import mimetypes
    mime_type, _ = mimetypes.guess_type(filepath)
    return mime_type or 'application/octet-stream'


def validate_source_file(filepath: str) -> bool:
    """Validate that file is a valid source code file"""
    if not os.path.exists(filepath):
        return False
    
    if not os.path.isfile(filepath):
        return False
    
    # Check file size (max 10MB for source files)
    file_size = os.path.getsize(filepath)
    if file_size > 10 * 1024 * 1024:
        return False
    
    # Check file extension
    ext = os.path.splitext(filepath)[1].lower()
    valid_extensions = {'.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', 
                       '.java', '.py', '.js', '.go', '.rs'}
    
    if ext not in valid_extensions:
        return False
    
    # Check if file is text (not binary)
    if is_binary_file(filepath):
        return False
    
    return True


def sanitize_filename(filename: str) -> str:
    """Sanitize filename for safe transmission"""
    import re
    
    # Remove path separators and dangerous characters
    filename = os.path.basename(filename)
    filename = re.sub(r'[<>:"|?*\x00-\x1f]', '_', filename)
    
    # Limit length
    if len(filename) > 255:
        name, ext = os.path.splitext(filename)
        max_name_len = 255 - len(ext)
        filename = name[:max_name_len] + ext
    
    return filename


def create_temp_file(prefix: str = "upload_", suffix: str = "") -> str:
    """Create a temporary file and return its path"""
    import tempfile
    
    fd, filepath = tempfile.mkstemp(prefix=prefix, suffix=suffix)
    os.close(fd)  # Close the file descriptor, we just need the path
    
    return filepath


class FileTransferStats:
    """Statistics tracking for file transfers"""
    
    def __init__(self):
        self.files_uploaded = 0
        self.files_downloaded = 0
        self.bytes_uploaded = 0
        self.bytes_downloaded = 0
        self.upload_errors = 0
        self.download_errors = 0
        self.total_upload_time = 0.0
        self.total_download_time = 0.0
    
    def record_upload(self, file_size: int, duration: float, success: bool):
        """Record upload statistics"""
        if success:
            self.files_uploaded += 1
            self.bytes_uploaded += file_size
            self.total_upload_time += duration
        else:
            self.upload_errors += 1
    
    def record_download(self, file_size: int, duration: float, success: bool):
        """Record download statistics"""
        if success:
            self.files_downloaded += 1
            self.bytes_downloaded += file_size
            self.total_download_time += duration
        else:
            self.download_errors += 1
    
    def get_upload_speed(self) -> float:
        """Get average upload speed in bytes per second"""
        if self.total_upload_time > 0:
            return self.bytes_uploaded / self.total_upload_time
        return 0.0
    
    def get_download_speed(self) -> float:
        """Get average download speed in bytes per second"""
        if self.total_download_time > 0:
            return self.bytes_downloaded / self.total_download_time
        return 0.0
    
    def print_summary(self):
        """Print transfer statistics summary"""
        print("\n=== File Transfer Statistics ===")
        print(f"Files uploaded: {self.files_uploaded}")
        print(f"Files downloaded: {self.files_downloaded}")
        print(f"Bytes uploaded: {self._format_bytes(self.bytes_uploaded)}")
        print(f"Bytes downloaded: {self._format_bytes(self.bytes_downloaded)}")
        print(f"Upload errors: {self.upload_errors}")
        print(f"Download errors: {self.download_errors}")
        
        upload_speed = self.get_upload_speed()
        download_speed = self.get_download_speed()
        
        if upload_speed > 0:
            print(f"Average upload speed: {self._format_bytes(upload_speed)}/s")
        if download_speed > 0:
            print(f"Average download speed: {self._format_bytes(download_speed)}/s")
    
    def _format_bytes(self, bytes_value: float) -> str:
        """Format bytes in human readable format"""
        for unit in ['B', 'KB', 'MB', 'GB']:
            if bytes_value < 1024.0:
                return f"{bytes_value:.1f} {unit}"
            bytes_value /= 1024.0
        return f"{bytes_value:.1f} TB"


class SecureFileTransfer:
    """Enhanced file transfer with encryption and compression"""
    
    def __init__(self, message_handler: MessageHandler, chunk_size: int = 64 * 1024):
        self.base_transfer = FileTransferManager(message_handler, chunk_size)
        self.encryption_enabled = False
        self.compression_enabled = False
        self.encryption_key = None
    
    def enable_encryption(self, key: bytes):
        """Enable file encryption during transfer"""
        self.encryption_enabled = True
        self.encryption_key = key
    
    def enable_compression(self):
        """Enable file compression during transfer"""
        self.compression_enabled = True
    
    def upload_file(self, filepath: str, 
                   progress_callback: Optional[Callable[[int, int], None]] = None) -> bool:
        """Upload file with optional encryption and compression"""
        
        # If no special features enabled, use base implementation
        if not self.encryption_enabled and not self.compression_enabled:
            return self.base_transfer.upload_file(filepath, progress_callback)
        
        # For encrypted/compressed transfers, we would need to:
        # 1. Process the file (compress/encrypt)
        # 2. Upload the processed file
        # 3. Send metadata about processing
        
        # This is a placeholder for the enhanced implementation
        print("Encrypted/compressed file transfer not yet implemented")
        return False
    
    def _compress_file(self, input_path: str, output_path: str) -> bool:
        """Compress file using gzip"""
        try:
            import gzip
            import shutil
            
            with open(input_path, 'rb') as f_in:
                with gzip.open(output_path, 'wb') as f_out:
                    shutil.copyfileobj(f_in, f_out)
            
            return True
        except Exception:
            return False
    
    def _encrypt_file(self, input_path: str, output_path: str, key: bytes) -> bool:
        """Encrypt file using AES (placeholder)"""
        # This would require a crypto library like cryptography
        # For now, just copy the file
        try:
            import shutil
            shutil.copy2(input_path, output_path)
            return True
        except Exception:
            return False


class BatchFileTransfer:
    """Utility for transferring multiple files"""
    
    def __init__(self, file_transfer: FileTransferManager):
        self.file_transfer = file_transfer
        self.stats = FileTransferStats()
    
    def upload_files(self, file_paths: list, 
                    progress_callback: Optional[Callable[[int, int, str], None]] = None) -> dict:
        """
        Upload multiple files
        
        Args:
            file_paths: List of file paths to upload
            progress_callback: Optional callback(current_file, total_files, filename)
        
        Returns:
            Dictionary with results for each file
        """
        results = {}
        
        for i, filepath in enumerate(file_paths):
            filename = os.path.basename(filepath)
            
            if progress_callback:
                progress_callback(i + 1, len(file_paths), filename)
            
            import time
            start_time = time.time()
            
            try:
                success = self.file_transfer.upload_file(filepath)
                file_size = os.path.getsize(filepath) if os.path.exists(filepath) else 0
                duration = time.time() - start_time
                
                self.stats.record_upload(file_size, duration, success)
                
                results[filepath] = {
                    'success': success,
                    'file_size': file_size,
                    'duration': duration
                }
                
            except Exception as e:
                duration = time.time() - start_time
                self.stats.record_upload(0, duration, False)
                
                results[filepath] = {
                    'success': False,
                    'error': str(e),
                    'duration': duration
                }
        
        return results
    
    def get_statistics(self) -> FileTransferStats:
        """Get transfer statistics"""
        return self.stats


# File type detection utilities
def detect_file_language(filepath: str) -> str:
    """Detect programming language from file"""
    ext = os.path.splitext(filepath)[1].lower()
    
    language_map = {
        '.c': 'C',
        '.h': 'C',
        '.cpp': 'C++',
        '.cc': 'C++',
        '.cxx': 'C++',
        '.hpp': 'C++',
        '.hxx': 'C++',
        '.java': 'Java',
        '.py': 'Python',
        '.js': 'JavaScript',
        '.go': 'Go',
        '.rs': 'Rust',
    }
    
    return language_map.get(ext, 'Unknown')


def get_recommended_compile_args(filepath: str) -> str:
    """Get recommended compiler arguments based on file type"""
    language = detect_file_language(filepath)
    
    args_map = {
        'C': '-Wall -Wextra -std=c99',
        'C++': '-Wall -Wextra -std=c++17',
        'Java': '-cp .',
        'Python': '',
        'JavaScript': '',
        'Go': '',
        'Rust': '--edition 2021',
    }
    
    return args_map.get(language, '')


def estimate_compilation_time(filepath: str) -> float:
    """Estimate compilation time based on file size and type"""
    try:
        file_size = os.path.getsize(filepath)
        language = detect_file_language(filepath)
        
        # Base time per KB for different languages (in seconds)
        time_per_kb = {
            'C': 0.01,
            'C++': 0.02,
            'Java': 0.015,
            'Python': 0.001,  # Interpreted
            'JavaScript': 0.001,  # Interpreted
            'Go': 0.005,
            'Rust': 0.03,
        }
        
        base_time = time_per_kb.get(language, 0.01)
        size_kb = file_size / 1024
        
        # Add base overhead
        estimated_time = max(1.0, size_kb * base_time + 2.0)
        
        return estimated_time
        
    except Exception:
        return 30.0  # Default estimate