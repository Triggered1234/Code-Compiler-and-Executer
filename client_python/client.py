#!/usr/bin/env python3
"""
Code Compiler & Executer - Python Client
Authors: Rares-Nicholas Popa & Adrian-Petru Enache
Date: April 2025
"""

import sys
import os
import socket
import struct
import time
import hashlib
import argparse
import json
import threading
from typing import Optional, Dict, Any, List, Tuple
from dataclasses import dataclass
from enum import IntEnum

from communication import MessageHandler, ProtocolError
from file_transfer import FileTransferManager


class Language(IntEnum):
    """Supported programming languages"""
    UNKNOWN = 0
    C = 1
    CPP = 2
    JAVA = 3
    PYTHON = 4
    JAVASCRIPT = 5
    GO = 6
    RUST = 7


class ExecutionMode(IntEnum):
    """Code execution modes"""
    COMPILE_ONLY = 0
    COMPILE_AND_RUN = 1
    INTERPRET_ONLY = 2
    SYNTAX_CHECK = 3


class JobStatus(IntEnum):
    """Job status codes"""
    QUEUED = 0
    COMPILING = 1
    RUNNING = 2
    COMPLETED = 3
    FAILED = 4
    CANCELLED = 5
    TIMEOUT = 6


@dataclass
class ClientConfig:
    """Client configuration"""
    server_host: str = "localhost"
    server_port: int = 8080
    timeout: int = 30
    retry_attempts: int = 3
    chunk_size: int = 64 * 1024  # 64KB
    client_name: str = "Python Client"
    verbose: bool = False
    debug: bool = False


@dataclass
class CompilationJob:
    """Represents a compilation job"""
    job_id: Optional[int] = None
    filename: str = ""
    language: Language = Language.UNKNOWN
    mode: ExecutionMode = ExecutionMode.COMPILE_AND_RUN
    compiler_args: str = ""
    execution_args: str = ""
    status: JobStatus = JobStatus.QUEUED
    submit_time: float = 0.0
    start_time: float = 0.0
    end_time: float = 0.0
    exit_code: int = 0
    output_size: int = 0
    error_size: int = 0


class CodeCompilerClient:
    """Main client class for Code Compiler & Executer"""
    
    def __init__(self, config: ClientConfig):
        self.config = config
        self.socket: Optional[socket.socket] = None
        self.message_handler: Optional[MessageHandler] = None
        self.file_transfer: Optional[FileTransferManager] = None
        self.connected = False
        self.client_id = None
        self.session_id = None
        self._lock = threading.Lock()
        
    def connect(self) -> bool:
        """Connect to the server"""
        try:
            if self.config.verbose:
                print(f"Connecting to {self.config.server_host}:{self.config.server_port}...")
            
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(self.config.timeout)
            
            # Connect to server
            self.socket.connect((self.config.server_host, self.config.server_port))
            
            # Initialize message handler
            self.message_handler = MessageHandler(self.socket, verbose=self.config.verbose)
            self.file_transfer = FileTransferManager(self.message_handler, self.config.chunk_size)
            
            # Send hello message
            if not self._send_hello():
                self.disconnect()
                return False
            
            self.connected = True
            if self.config.verbose:
                print("Connected successfully!")
            
            return True
            
        except Exception as e:
            print(f"Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from server"""
        with self._lock:
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            
            self.connected = False
            self.message_handler = None
            self.file_transfer = None
            
            if self.config.verbose:
                print("Disconnected from server")
    
    def _send_hello(self) -> bool:
        """Send hello message to server"""
        try:
            import platform
            
            hello_data = {
                'client_name': self.config.client_name,
                'client_platform': f"{platform.system()} {platform.release()}",
                'client_version_major': 1,
                'client_version_minor': 0,
                'client_version_patch': 0,
                'capabilities': 0x0004  # FILE_TRANSFER capability
            }
            
            response = self.message_handler.send_hello(hello_data)
            if response and response.get('status') == 'ok':
                if self.config.verbose:
                    print(f"Handshake successful with server")
                return True
            
            return False
            
        except Exception as e:
            print(f"Hello handshake failed: {e}")
            return False
    
    def detect_language(self, filename: str) -> Language:
        """Detect programming language from file extension"""
        ext = os.path.splitext(filename)[1].lower()
        
        language_map = {
            '.c': Language.C,
            '.cpp': Language.CPP,
            '.cc': Language.CPP,
            '.cxx': Language.CPP,
            '.java': Language.JAVA,
            '.py': Language.PYTHON,
            '.js': Language.JAVASCRIPT,
            '.go': Language.GO,
            '.rs': Language.RUST,
        }
        
        return language_map.get(ext, Language.UNKNOWN)
    
    def submit_file(self, filename: str, compiler_args: str = "", 
                   execution_args: str = "", mode: ExecutionMode = ExecutionMode.COMPILE_AND_RUN) -> Optional[CompilationJob]:
        """Submit a source file for compilation/execution"""
        if not self.connected:
            print("Not connected to server")
            return None
        
        if not os.path.exists(filename):
            print(f"File not found: {filename}")
            return None
        
        try:
            # Detect language
            language = self.detect_language(filename)
            if language == Language.UNKNOWN:
                print(f"Unsupported file type: {filename}")
                return None
            
            # Create job
            job = CompilationJob(
                filename=os.path.basename(filename),
                language=language,
                mode=mode,
                compiler_args=compiler_args,
                execution_args=execution_args,
                submit_time=time.time()
            )
            
            if self.config.verbose:
                print(f"Submitting {filename} for {language.name} {mode.name}...")
            
            # Upload file
            if not self.file_transfer.upload_file(filename):
                print("File upload failed")
                return None
            
            # Send compile request
            compile_data = {
                'filename': job.filename,
                'language': job.language.value,
                'mode': job.mode.value,
                'compiler_args': job.compiler_args,
                'execution_args': job.execution_args
            }
            
            response = self.message_handler.send_compile_request(compile_data)
            if not response:
                print("Compilation request failed")
                return None
            
            job.job_id = response.get('job_id')
            job.status = JobStatus(response.get('status', JobStatus.QUEUED))
            
            if self.config.verbose:
                print(f"Job submitted with ID: {job.job_id}")
            
            return job
            
        except Exception as e:
            print(f"File submission failed: {e}")
            return None
    
    def get_job_status(self, job_id: int) -> Optional[Dict[str, Any]]:
        """Get status of a compilation job"""
        if not self.connected:
            print("Not connected to server")
            return None
        
        try:
            response = self.message_handler.send_status_request(job_id)
            return response
            
        except Exception as e:
            print(f"Status request failed: {e}")
            return None
    
    def get_job_results(self, job_id: int) -> Optional[Dict[str, Any]]:
        """Get results of a completed job"""
        if not self.connected:
            print("Not connected to server")
            return None
        
        try:
            response = self.message_handler.send_result_request(job_id)
            return response
            
        except Exception as e:
            print(f"Result request failed: {e}")
            return None
    
    def wait_for_job(self, job: CompilationJob, poll_interval: float = 1.0) -> bool:
        """Wait for a job to complete and show progress"""
        if not job or not job.job_id:
            return False
        
        if self.config.verbose:
            print(f"Waiting for job {job.job_id} to complete...")
        
        while True:
            status = self.get_job_status(job.job_id)
            if not status:
                return False
            
            job.status = JobStatus(status.get('status', JobStatus.QUEUED))
            
            if self.config.verbose:
                progress = status.get('progress', 0)
                print(f"Job {job.job_id}: {job.status.name} ({progress}%)")
            
            if job.status in [JobStatus.COMPLETED, JobStatus.FAILED, JobStatus.CANCELLED, JobStatus.TIMEOUT]:
                break
            
            time.sleep(poll_interval)
        
        # Get final results
        if job.status == JobStatus.COMPLETED:
            results = self.get_job_results(job.job_id)
            if results:
                job.exit_code = results.get('exit_code', 0)
                job.output_size = results.get('output_size', 0)
                job.error_size = results.get('error_size', 0)
                
                # Display output if available
                output = results.get('output', '')
                error = results.get('error', '')
                
                if output:
                    print("\n--- Program Output ---")
                    print(output)
                
                if error:
                    print("\n--- Error Output ---")
                    print(error)
                
                return job.exit_code == 0
        
        return False
    
    def compile_and_run(self, filename: str, compiler_args: str = "", 
                       execution_args: str = "", wait: bool = True) -> bool:
        """Compile and run a source file (convenience method)"""
        job = self.submit_file(filename, compiler_args, execution_args, ExecutionMode.COMPILE_AND_RUN)
        if not job:
            return False
        
        if wait:
            return self.wait_for_job(job)
        
        return True
    
    def compile_only(self, filename: str, compiler_args: str = "") -> bool:
        """Compile a source file without execution"""
        job = self.submit_file(filename, compiler_args, "", ExecutionMode.COMPILE_ONLY)
        if not job:
            return False
        
        return self.wait_for_job(job)
    
    def ping_server(self) -> bool:
        """Ping the server to test connectivity"""
        if not self.connected:
            return False
        
        try:
            response = self.message_handler.send_ping()
            return response is not None
            
        except Exception as e:
            if self.config.verbose:
                print(f"Ping failed: {e}")
            return False


def create_config_from_args(args) -> ClientConfig:
    """Create client configuration from command line arguments"""
    config = ClientConfig()
    
    if hasattr(args, 'host') and args.host:
        config.server_host = args.host
    
    if hasattr(args, 'port') and args.port:
        config.server_port = args.port
    
    if hasattr(args, 'timeout') and args.timeout:
        config.timeout = args.timeout
    
    if hasattr(args, 'verbose') and args.verbose:
        config.verbose = True
    
    if hasattr(args, 'debug') and args.debug:
        config.debug = True
    
    return config


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Code Compiler & Executer Python Client")
    
    # Connection options
    parser.add_argument('--host', default='localhost', 
                       help='Server hostname (default: localhost)')
    parser.add_argument('--port', type=int, default=8080,
                       help='Server port (default: 8080)')
    parser.add_argument('--timeout', type=int, default=30,
                       help='Connection timeout in seconds (default: 30)')
    
    # Action options
    parser.add_argument('--file', '-f', 
                       help='Source file to compile/execute')
    parser.add_argument('--compiler-args', default='',
                       help='Arguments to pass to compiler')
    parser.add_argument('--execution-args', default='',
                       help='Arguments to pass to executed program')
    parser.add_argument('--mode', choices=['compile', 'run', 'interpret', 'check'],
                       default='run', help='Execution mode (default: run)')
    
    # Testing options
    parser.add_argument('--ping', action='store_true',
                       help='Ping server and exit')
    parser.add_argument('--test-connection', action='store_true',
                       help='Test connection and exit')
    
    # Output options
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose output')
    parser.add_argument('--debug', action='store_true',
                       help='Debug output')
    parser.add_argument('--version', action='version', version='Python Client 1.0.0')
    
    args = parser.parse_args()
    
    # Create configuration
    config = create_config_from_args(args)
    
    # Create client
    client = CodeCompilerClient(config)
    
    try:
        # Connect to server
        if not client.connect():
            print("Failed to connect to server")
            return 1
        
        # Handle ping request
        if args.ping:
            if client.ping_server():
                print("Server is responding")
                return 0
            else:
                print("Server is not responding")
                return 1
        
        # Handle connection test
        if args.test_connection:
            print("Connection test successful")
            return 0
        
        # Handle file compilation/execution
        if args.file:
            mode_map = {
                'compile': ExecutionMode.COMPILE_ONLY,
                'run': ExecutionMode.COMPILE_AND_RUN,
                'interpret': ExecutionMode.INTERPRET_ONLY,
                'check': ExecutionMode.SYNTAX_CHECK
            }
            
            mode = mode_map.get(args.mode, ExecutionMode.COMPILE_AND_RUN)
            
            if mode == ExecutionMode.COMPILE_ONLY:
                success = client.compile_only(args.file, args.compiler_args)
            else:
                success = client.compile_and_run(args.file, args.compiler_args, 
                                                args.execution_args)
            
            return 0 if success else 1
        
        # Interactive mode
        print("Code Compiler & Executer Python Client")
        print("Type 'help' for available commands, 'quit' to exit")
        
        while True:
            try:
                command = input("client> ").strip()
                
                if not command:
                    continue
                
                if command in ['quit', 'exit']:
                    break
                elif command == 'help':
                    print("Available commands:")
                    print("  compile <file> [args]  - Compile file")
                    print("  run <file> [args]      - Compile and run file")
                    print("  ping                   - Ping server")
                    print("  quit/exit              - Exit client")
                elif command == 'ping':
                    if client.ping_server():
                        print("Server is responding")
                    else:
                        print("Server is not responding")
                elif command.startswith('compile '):
                    parts = command.split(' ', 1)
                    if len(parts) > 1:
                        client.compile_only(parts[1])
                elif command.startswith('run '):
                    parts = command.split(' ', 1)
                    if len(parts) > 1:
                        client.compile_and_run(parts[1])
                else:
                    print(f"Unknown command: {command}")
                    
            except KeyboardInterrupt:
                print("\nExiting...")
                break
            except EOFError:
                break
        
        return 0
        
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return 1
    except Exception as e:
        print(f"Error: {e}")
        return 1
    finally:
        client.disconnect()


if __name__ == '__main__':
    sys.exit(main())