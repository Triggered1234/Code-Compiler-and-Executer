#!/usr/bin/env python3
"""
@file client.py
@brief Cross-platform client for Code Compiler & Executor Server
@author Rares-Nicholas Popa & Adrian-Petru Enache
@version 1.0.0

@details This application provides a cross-platform Python client for submitting
C source code to the Code Compiler & Executor Server. It offers the same
functionality as the C++ client but with cross-platform compatibility
(Windows, macOS, Linux) and additional helper features.

@copyright This project is for educational purposes as part of the PCD course.
"""

import socket
import sys
import os

# Server configuration constants
SERVER_IP = "127.0.0.1"    #: Default server IP address
PORT = 8080                #: Regular client server port
BUFFER_SIZE = 4096         #: Maximum buffer size for network communications

class CrossPlatformClient:
    """
    Cross-platform client class for code submission and execution.
    
    This class provides an interface for users to submit C source code
    to the server for compilation and execution. It includes additional
    features like built-in help and sample code.
    
    Attributes:
        sock: Socket connection to the server
    """
    
    def __init__(self):
        """
        Initialize the CrossPlatformClient.
        
        Sets up the client with no active socket connection.
        """
        self.sock = None
    
    def connect_to_server(self):
        """
        Connect to the server.
        
        Establishes a TCP connection to the server's regular client port.
        
        Returns:
            bool: True if connection successful, False otherwise
            
        Note:
            Displays connection status and error messages.
        """
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((SERVER_IP, PORT))
            print(f"Error sending code: {e}")
    
    def load_file(self, filename):
        """
        Load C source code from a file.
        
        Args:
            filename (str): Path to the file containing C source code
            
        Returns:
            str or None: File contents if successful, None on error
            
        Note:
            Handles file not found and other I/O errors gracefully.
        """
        try:
            with open(filename, 'r') as file:
                code = file.read()
                print(f"Loaded code from file: {filename}")
                return code
        except FileNotFoundError:
            print(f"Error: File '{filename}' not found")
            return None
        except Exception as e:
            print(f"Error reading file: {e}")
            return None
    
    def run(self):
        """
        Main client loop.
        
        Provides interactive interface for code submission with cross-platform
        compatibility. Supports interactive code entry, file loading, and
        built-in help with sample code.
        
        Features:
            - Interactive multi-line code entry (end with 'END')
            - File loading with 'load <filename>' command
            - Built-in help with sample code
            - Graceful error handling and user feedback
            - Cross-platform compatibility
        """
        """
        Load C source code from a file.
        
        Args:
            filename (str): Path to the file containing C source code
            
        Returns:
            str or None: File contents if successful, None on error
            
        Note:
            Handles file not found and other I/O errors gracefully.
        """f"Connected to server on port {PORT}")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False
    
    def send_code(self, code):
        """
        Send C source code to server for compilation and execution.
        
        Args:
            code (str): The C source code to compile and execute
            
        Note:
            Displays formatted execution results and handles network errors.
        """
        try:
            self.sock.send(code.encode('utf-8'))
            response = self.sock.recv(BUFFER_SIZE).decode('utf-8')
            
            print("\n=== EXECUTION RESULT ===")
            print(response)
            print("========================")
            
        except Exception as e:
            print(f"Error sending code: {e}")
    
    def load_file(self, filename):
        try:
            with open(filename, 'r') as file:
                code = file.read()
                print(f"Loaded code from file: {filename}")
                return code
        except FileNotFoundError:
            print(f"Error: File '{filename}' not found")
            return None
        except Exception as e:
            print(f"Error reading file: {e}")
            return None
    
    def run(self):
        if not self.connect_to_server():
            return
        
        print("\nCross-Platform Client - Code Compiler & Executor")
        print("Commands:")
        print("1. Type C code directly (end with 'END' on a new line)")
        print("2. 'load <filename>' - Load code from file")
        print("3. 'help' - Show sample code")
        print("4. 'quit' - Exit")
        
        while True:
            try:
                command = input("\nClient> ").strip()
                
                if command == "quit":
                    self.sock.send(b"QUIT")
                    break
                
                if command == "help":
                    self.show_sample_code()
                    continue
                
                if command.startswith("load "):
                    filename = command[5:].strip()
                    code = self.load_file(filename)
                    if code:
                        self.send_code(code)
                    continue
                
                # Multi-line code input
                print("Enter your C code (type 'END' on a new line to finish):")
                lines = []
                while True:
                    try:
                        line = input()
                        if line == "END":
                            break
                        lines.append(line)
                    except EOFError:
                        break
                
                code = "\n".join(lines)
                if code.strip():
                    self.send_code(code)
                
            except KeyboardInterrupt:
                print("\nExiting...")
                break
            except Exception as e:
                print(f"Error: {e}")
        
        if self.sock:
            self.sock.close()
        print("Disconnected from server.")
    
    def show_sample_code(self):
        """
        Display sample C code and optionally send it to server.
        
        Shows a simple "Hello World" example and asks the user if they
        want to submit it for compilation and execution.
        
        Note:
            This is a helper function to demonstrate the system functionality.
        """
        print("\n=== SAMPLE C CODE ===")
        print("#include <stdio.h>")
        print("int main() {")
        print("    printf(\"Hello from Python client!\\n\");")
        print("    return 0;")
        print("}")
        print("====================")
        
        answer = input("Do you want to send this sample code? (y/n): ")
        if answer.lower() in ['y', 'yes']:
            sample_code = """#include <stdio.h>
int main() {
    printf("Hello from Python client!\\n");
    return 0;
}"""
            self.send_code(sample_code)

def main():
    """
    Main function for cross-platform client.
    
    Entry point for the Python client application. Displays system
    information and creates a CrossPlatformClient instance to run
    the interactive interface.
    
    Note:
        Shows platform and Python version information for debugging
        cross-platform compatibility.
    """
    print("Code Compiler & Executor - Cross-Platform Client (Python)")
    print("=========================================================")
    print(f"Platform: {sys.platform}")
    print(f"Python version: {sys.version}")
    
    client = CrossPlatformClient()
    client.run()

if __name__ == "__main__":
    main()