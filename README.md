# Code Compiler & Executor

A simple client-server application that allows users to submit C code for compilation and execution on a remote server.

## Project Structure

```
Code-Compiler-and-Executer/
├── src/
│   ├── server.c             # Server application (C, UNIX)
│   ├── admin_client.cpp     # Admin client (C++)
│   ├── client.cpp          # Regular client (C++)  
│   └── client.py           # Cross-platform client (Python)
├── cmake/
│   └── FindThreads.cmake   # Custom CMake module
├── build/                  # Build directory (generated)
├── CMakeLists.txt          # Main CMake configuration
├── build.sh               # Build script
├── test.c                 # Sample test code
└── README.md              # This file
```

## Components

### 1. Server Application (`server.c`)
- **Language**: C
- **Platform**: UNIX/Linux
- **Ports**: 8080 (regular clients), 8081 (admin clients)
- **Features**:
  - Multi-threaded server handling concurrent clients
  - Code compilation using GCC
  - Program execution with timeout (5 seconds)
  - Activity logging
  - Statistics tracking

### 2. Admin Client (`admin_client.cpp`)
- **Language**: C++
- **Platform**: UNIX/Linux
- **Port**: 8081
- **Features**:
  - Server status monitoring
  - Log file viewing
  - Server shutdown capability
  - Real-time statistics

### 3. Regular Client (`client.cpp`)
- **Language**: C++
- **Platform**: UNIX/Linux
- **Port**: 8080
- **Features**:
  - Interactive code submission
  - File loading capability
  - Real-time compilation results
  - Multi-line code input

### 4. Cross-Platform Client (`client.py`)
- **Language**: Python
- **Platform**: Cross-platform (Windows, macOS, Linux)
- **Port**: 8080
- **Features**:
  - Same functionality as regular client
  - Cross-platform compatibility
  - Built-in help and sample code
  - File loading support

## Prerequisites

- CMake 3.10 or higher
- GCC/G++ compiler
- pthread library
- Python 3.x (for cross-platform client)
- Doxygen (optional, for documentation generation)
- UNIX/Linux environment (for server and C++ clients)

## Quick Start

### Option 1: Using the build script (Recommended)
```bash
# Make script executable
chmod +x build.sh

# Build the project
./build.sh build

# Start the server (Terminal 1)
./build.sh run-server

# Connect with admin client (Terminal 2)
./build.sh run-admin

# Connect with regular client (Terminal 3)
./build.sh run-client

# Or use Python client (Terminal 4)
./build.sh run-python
```

### Option 2: Using CMake directly
```bash
# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make

# Run applications
make run-server     # Terminal 1
make run-admin      # Terminal 2  
make run-client     # Terminal 3
make run-python-client  # Terminal 4

# Generate documentation
make docs               # Creates docs/html/index.html
```

## Usage Examples

### Regular Client Usage
1. Start the client
2. Enter C code line by line
3. Type `END` to finish and execute
4. Or use `load filename.c` to load from file

### Admin Client Commands
- `STATUS` - View server statistics
- `LOGS` - View server activity logs  
- `SHUTDOWN` - Shutdown the server
- `QUIT` - Disconnect from server

### Sample C Code
```c
#include <stdio.h>
int main() {
    printf("Hello World!\n");
    return 0;
}
```

## Architecture

### Threading Model
- **Main Thread**: Coordinates other threads
- **Regular Client Thread**: Handles code compilation requests
- **Admin Thread**: Handles administration requests
- **Client Handler Threads**: One per connected client

### Communication Protocol
- Simple text-based protocol
- Commands: QUIT, STATUS, LOGS, SHUTDOWN
- Code submission: Send C source code directly
- Responses: Compilation results or error messages

## Security Notes

This is a minimal implementation for educational purposes. In production, you would need:
- Input validation and sanitization
- Secure sandboxing for code execution
- Authentication and authorization
- Resource limits and quotas
- Proper error handling

## File Structure

### Generated Files
- `server.log` - Server activity log
- `temp_code.c` - Temporary source files
- `temp_program` - Temporary executables
- `compile_error.log` - Compilation error logs

### Directories
- `processing/` - Processing workspace
- `outgoing/` - Output files

## Building Individual Components

```bash
# Using build script
./build.sh clean          # Clean build
./build.sh rebuild        # Clean and rebuild
./build.sh install        # Install system-wide

# Using CMake directly
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug  # Debug build
make server                        # Build only server
make admin_client                  # Build only admin client
make client                        # Build only regular client
make show-help                     # Show available targets

# Documentation
make docs                          # Generate Doxygen docs  
make docs-clean                    # Clean documentation
```

## Documentation

The project includes comprehensive Doxygen documentation for all source files:

### Generate Documentation
```bash
# Using build script
./build.sh docs

# Using CMake directly
cd build && make docs
```

### View Documentation
- Open `docs/html/index.html` in your web browser
- Contains API documentation for all functions and classes
- Includes file documentation, function parameters, and return values
- Cross-referenced with source code

### Documentation Features
- **Complete API documentation** for all C/C++ functions
- **Python docstrings** for the cross-platform client
- **File-level documentation** with project details
- **Cross-references** between functions and files
- **Source code browser** with syntax highlighting
- **Class diagrams** and dependency graphs (if Graphviz installed)

## Troubleshooting

### Common Issues

1. **Port already in use**
   - Change PORT and ADMIN_PORT constants in source files
   - Or kill existing processes: `killall server`

2. **GCC not found**
   - Install build-essential: `sudo apt-get install build-essential cmake`

3. **Permission denied**
   - Make files executable: `chmod +x build.sh`
   - Or in build directory: `chmod +x bin/*`

4. **Connection refused**
   - Ensure server is running before starting clients
   - Check firewall settings

## License

This project is for educational purposes as part of the PCD course.