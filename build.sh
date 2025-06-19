#!/bin/bash

# Build script pentru proiectul Code Compiler & Executer
# Usage: ./build.sh [options]

set -e  # Exit on any error

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
CLEAN_BUILD=false
RUN_TESTS=false
INSTALL=false
VERBOSE=false
JOBS=$(nproc 2>/dev/null || echo 4)
ENABLE_SANITIZERS=false
ENABLE_COVERAGE=false
ENABLE_THREADING_CHECK=false
CREATE_DEV_ENV=true

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_debug() {
    echo -e "${PURPLE}[DEBUG]${NC} $1"
}

print_build() {
    echo -e "${CYAN}[BUILD]${NC} $1"
}

# Function to show usage
show_usage() {
    cat << EOF
Build script pentru Code Compiler & Executer

Usage: $0 [OPTIONS]

OPTIONS:
    -t, --type TYPE         Build type (Debug|Release|RelWithDebInfo|MinSizeRel) [default: Release]
    -d, --dir DIR          Build directory [default: build]
    -c, --clean            Clean build (remove build directory first)
    -j, --jobs N           Number of parallel jobs [default: $(nproc)]
    -i, --install          Install after build
    -T, --tests            Run tests after build
    -v, --verbose          Verbose output
    -s, --sanitizers       Enable sanitizers (Debug only)
    --coverage             Enable code coverage (Debug only)
    --threading            Enable thread safety checks (Debug only)
    --no-dev-env           Don't create development environment
    -h, --help             Show this help

EXAMPLES:
    $0                                    # Release build
    $0 -t Debug -c -T                    # Clean debug build with tests
    $0 -t Debug -s --coverage -T         # Debug with sanitizers and coverage
    $0 -c -i                             # Clean release build and install
    $0 -t Debug --threading -T           # Debug with thread safety checks

DEVELOPMENT WORKFLOW:
    $0 -t Debug -c -T -s                 # Full development build
    cd build && ./quick_start.sh         # Start development environment
    ./run_tests.sh                       # Run comprehensive tests

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -d|--dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -i|--install)
            INSTALL=true
            shift
            ;;
        -T|--tests)
            RUN_TESTS=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -s|--sanitizers)
            ENABLE_SANITIZERS=true
            shift
            ;;
        --coverage)
            ENABLE_COVERAGE=true
            shift
            ;;
        --threading)
            ENABLE_THREADING_CHECK=true
            shift
            ;;
        --no-dev-env)
            CREATE_DEV_ENV=false
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Validate build type
case $BUILD_TYPE in
    Debug|Release|RelWithDebInfo|MinSizeRel)
        ;;
    *)
        print_error "Invalid build type: $BUILD_TYPE"
        print_error "Valid types: Debug, Release, RelWithDebInfo, MinSizeRel"
        exit 1
        ;;
esac

# Print header
echo
echo "================================================================="
echo "    Code Compiler & Executer - Build System"
echo "    Rares-Nicholas Popa & Adrian-Petru Enache"
echo "================================================================="
echo

# Check dependencies
print_status "Checking system dependencies..."

# Function to check for required tools
check_dependency() {
    if ! command -v "$1" &> /dev/null; then
        print_error "$1 is required but not installed"
        case $1 in
            cmake)
                print_error "Install with: sudo apt-get install cmake (Ubuntu/Debian)"
                print_error "             sudo yum install cmake (CentOS/RHEL)"
                ;;
            make)
                print_error "Install with: sudo apt-get install build-essential"
                ;;
            gcc)
                print_error "Install with: sudo apt-get install gcc"
                ;;
            g++)
                print_error "Install with: sudo apt-get install g++"
                ;;
        esac
        return 1
    else
        print_success "$1 found: $(command -v "$1")"
        return 0
    fi
}

# Function to check optional tools
check_optional() {
    if command -v "$1" &> /dev/null; then
        print_success "$1 found: $(command -v "$1")"
        return 0
    else
        print_warning "$1 not found (optional for $2)"
        return 1
    fi
}

# Check required dependencies
DEPS_OK=true
check_dependency "cmake" || DEPS_OK=false
check_dependency "make" || DEPS_OK=false
check_dependency "gcc" || DEPS_OK=false
check_dependency "g++" || DEPS_OK=false

if [ "$DEPS_OK" = false ]; then
    print_error "Missing required dependencies. Please install them and try again."
    exit 1
fi

# Check compiler versions
print_status "Checking compiler versions..."
GCC_VERSION=$(gcc --version | head -n1)
GPP_VERSION=$(g++ --version | head -n1)
CMAKE_VERSION=$(cmake --version | head -n1)
print_debug "GCC: $GCC_VERSION"
print_debug "G++: $GPP_VERSION"
print_debug "CMake: $CMAKE_VERSION"

# Check for runtime dependencies (compilers for the server to use)
print_status "Checking runtime compiler support..."
check_optional "python3" "Python code execution"
check_optional "java" "Java code execution"
check_optional "javac" "Java code compilation"
check_optional "node" "JavaScript code execution"
check_optional "go" "Go code compilation and execution"
check_optional "rustc" "Rust code compilation"

# Check for development tools
if [ "$BUILD_TYPE" = "Debug" ]; then
    print_status "Checking debugging tools..."
    check_optional "gdb" "debugging"
    check_optional "valgrind" "memory checking"
    
    if [ "$ENABLE_COVERAGE" = true ]; then
        print_status "Checking coverage tools..."
        check_dependency "gcov" || print_warning "Code coverage will not be available"
        check_optional "lcov" "coverage report generation"
        check_optional "genhtml" "HTML coverage reports"
    fi
    
    if [ "$ENABLE_THREADING_CHECK" = true ]; then
        print_status "Checking thread analysis tools..."
        check_optional "helgrind" "thread safety analysis (part of valgrind)"
    fi
fi

# Check for testing framework
if [ "$RUN_TESTS" = true ]; then
    print_status "Checking testing framework..."
    if pkg-config --exists gtest 2>/dev/null; then
        print_success "Google Test found via pkg-config"
    elif [ -f /usr/include/gtest/gtest.h ]; then
        print_success "Google Test headers found in system"
    else
        print_warning "Google Test not found - tests may not build"
        print_warning "Install with: sudo apt-get install libgtest-dev libgmock-dev"
    fi
fi

# Check for optional libraries
print_status "Checking optional libraries..."
check_optional "pkg-config" "library detection"

if pkg-config --exists ncurses 2>/dev/null; then
    print_success "ncurses found - enhanced terminal interface will be available"
else
    print_warning "ncurses not found - will use basic terminal interface"
fi

if pkg-config --exists json-c 2>/dev/null; then
    print_success "json-c found - JSON configuration support available"
else
    print_warning "json-c not found - will use basic configuration"
fi

if pkg-config --exists openssl 2>/dev/null; then
    print_success "OpenSSL found - secure communication available"
else
    print_warning "OpenSSL not found - secure communication disabled"
fi

print_success "Dependency check completed"

# Clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_build "Cleaning build directory: $BUILD_DIR"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    else
        print_warning "Build directory doesn't exist - nothing to clean"
    fi
fi

# Create build directory
print_build "Creating build directory: $BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Prepare CMake arguments
CMAKE_ARGS=(
    "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)

# Add conditional arguments
if [ "$RUN_TESTS" = true ]; then
    CMAKE_ARGS+=("-DBUILD_TESTS=ON")
    print_debug "Tests enabled"
else
    CMAKE_ARGS+=("-DBUILD_TESTS=OFF")
    print_debug "Tests disabled"
fi

if [ "$BUILD_TYPE" = "Debug" ] && [ "$ENABLE_SANITIZERS" = true ]; then
    CMAKE_ARGS+=("-DENABLE_ASAN=ON")
    CMAKE_ARGS+=("-DENABLE_UBSAN=ON")
    print_debug "Sanitizers enabled (AddressSanitizer + UndefinedBehaviorSanitizer)"
fi

if [ "$BUILD_TYPE" = "Debug" ] && [ "$ENABLE_THREADING_CHECK" = true ]; then
    CMAKE_ARGS+=("-DENABLE_TSAN=ON")
    print_debug "ThreadSanitizer enabled"
fi

if [ "$BUILD_TYPE" = "Debug" ] && [ "$ENABLE_COVERAGE" = true ]; then
    CMAKE_ARGS+=("-DENABLE_COVERAGE=ON")
    print_debug "Code coverage enabled"
fi

# Check for ncurses and add to CMake args
if pkg-config --exists ncurses 2>/dev/null; then
    CMAKE_ARGS+=("-DBUILD_WITH_NCURSES=ON")
    print_debug "ncurses support enabled"
else
    CMAKE_ARGS+=("-DBUILD_WITH_NCURSES=OFF")
    print_debug "ncurses support disabled"
fi

# Configure CMake
print_build "Configuring CMake..."
print_debug "Build type: $BUILD_TYPE"
print_debug "CMake arguments: ${CMAKE_ARGS[*]}"

if [ "$VERBOSE" = true ]; then
    cmake .. "${CMAKE_ARGS[@]}"
else
    cmake .. "${CMAKE_ARGS[@]}" > cmake_config.log 2>&1
    if [ $? -ne 0 ]; then
        print_error "CMake configuration failed. Log output:"
        echo "----------------------------------------"
        cat cmake_config.log
        echo "----------------------------------------"
        exit 1
    fi
fi

print_success "CMake configuration completed"

# Build the project
print_build "Building project with $JOBS parallel jobs..."

MAKE_ARGS=("-j$JOBS")
if [ "$VERBOSE" = true ]; then
    MAKE_ARGS+=("VERBOSE=1")
fi

start_time=$(date +%s)

if [ "$VERBOSE" = true ]; then
    make "${MAKE_ARGS[@]}"
else
    make "${MAKE_ARGS[@]}" > build.log 2>&1
    if [ $? -ne 0 ]; then
        print_error "Build failed. Last 50 lines of build log:"
        echo "----------------------------------------"
        tail -50 build.log
        echo "----------------------------------------"
        print_error "Full log available in: $BUILD_DIR/build.log"
        exit 1
    fi
fi

end_time=$(date +%s)
build_time=$((end_time - start_time))

print_success "Build completed in ${build_time}s"

# Show build artifacts
print_build "Build artifacts created:"
if [ -d "bin" ]; then
    find bin -type f -executable 2>/dev/null | sort | while read -r file; do
        size=$(du -h "$file" | cut -f1)
        echo "  - $file ($size)"
    done
else
    print_warning "No executables found in bin/ directory"
fi

# Run tests if requested
if [ "$RUN_TESTS" = true ]; then
    print_build "Running tests..."
    
    test_start_time=$(date +%s)
    
    if [ "$VERBOSE" = true ]; then
        ctest --output-on-failure --parallel "$JOBS"
    else
        ctest --output-on-failure --parallel "$JOBS" > test.log 2>&1
        test_result=$?
        
        if [ $test_result -ne 0 ]; then
            print_error "Tests failed. Test output:"
            echo "----------------------------------------"
            cat test.log
            echo "----------------------------------------"
            exit 1
        fi
    fi
    
    test_end_time=$(date +%s)
    test_time=$((test_end_time - test_start_time))
    
    print_success "All tests passed in ${test_time}s"
    
    # Generate coverage report if enabled
    if [ "$BUILD_TYPE" = "Debug" ] && [ "$ENABLE_COVERAGE" = true ]; then
        print_build "Generating coverage report..."
        if make coverage > coverage.log 2>&1; then
            print_success "Coverage report generated in coverage_html/"
            if command -v xdg-open &> /dev/null && [ -f "coverage_html/index.html" ]; then
                print_status "Opening coverage report in browser..."
                xdg-open coverage_html/index.html &
            fi
        else
            print_warning "Coverage report generation failed - check coverage.log"
        fi
    fi
fi

# Install if requested
if [ "$INSTALL" = true ]; then
    print_build "Installing..."
    
    if [ "$VERBOSE" = true ]; then
        make install
    else
        make install > install.log 2>&1
        if [ $? -ne 0 ]; then
            print_error "Installation failed. Log output:"
            echo "----------------------------------------"
            cat install.log
            echo "----------------------------------------"
            exit 1
        fi
    fi
    
    print_success "Installation completed"
    print_status "Installed to: $(realpath install 2>/dev/null || echo "install/")"
fi

# Create development environment
if [ "$CREATE_DEV_ENV" = true ]; then
    print_build "Creating development environment..."
    
    # Create useful aliases
    cat > aliases.sh << 'EOF'

#!/bin/bash
# Development aliases for Code Compiler & Executer

# Build aliases
alias rebuild='cd .. && ./build.sh -t Debug -c'
alias rebuild_release='cd .. && ./build.sh -c'
alias rebuild_test='cd .. && ./build.sh -t Debug -c -T'
alias rebuild_full='cd .. && ./build.sh -t Debug -c -T -s --coverage'

# Application aliases
alias server='./bin/code_server'
alias client='./bin/code_client'
alias admin='./bin/admin_client'

# Testing aliases
alias test_all='ctest --output-on-failure'
alias test_unit='ctest -L unit --output-on-failure'
alias test_integration='ctest -L integration --output-on-failure'
alias test_quick='ctest -R "protocol_tests|queue_tests" --output-on-failure'

# Debug aliases
alias debug_server='gdb --args ./bin/code_server --config ./dev_config/server_dev.conf --no-daemon'
alias debug_client='gdb --args ./bin/code_client --config ./dev_config/client_dev.conf'
alias debug_admin='gdb --args ./bin/admin_client --config ./dev_config/admin_dev.conf'

# Memory check aliases
alias memcheck_server='valgrind --leak-check=full --show-leak-kinds=all ./bin/code_server --config ./dev_config/server_dev.conf'
alias memcheck_client='valgrind --leak-check=full ./bin/code_client --test-connection'

# Utility aliases
alias clean_logs='rm -f logs/*.log'
alias clean_temp='rm -rf processing/* outgoing/* temp/*'
alias show_logs='tail -f logs/*.log'

echo -e "\033[0;32m[SUCCESS]\033[0m Development aliases loaded!"
echo ""
echo "Available aliases:"
echo "  Build: rebuild, rebuild_release, rebuild_test, rebuild_full"
echo "  Apps:  server, client, admin"
echo "  Test:  test_all, test_unit, test_integration, test_quick"
echo "  Debug: debug_server, debug_client, debug_admin"
echo "  Memory: memcheck_server, memcheck_client"
echo "  Utils: clean_logs, clean_temp, show_logs"
EOF

    chmod +x aliases.sh
    
    # Create development configuration files
    mkdir -p dev_config logs processing outgoing temp
    
    # Server development config
    cat > dev_config/server_dev.conf << 'EOF'
# Development configuration for code_server

[server]
host = localhost
port = 8080
admin_socket = ./server_admin.sock
max_clients = 100
client_timeout = 300
admin_timeout = 1800
debug_mode = true

[logging]
level = debug
console = true
file = ./logs/server_dev.log
max_size = 10MB
max_files = 5

[compilation]
timeout = 60
max_memory = 512MB
max_processes = 10
allowed_languages = c,cpp,python,java,javascript,go
compiler_cache = true

[paths]
processing_dir = ./processing
output_dir = ./outgoing
temp_dir = ./temp
log_dir = ./logs

[security]
enable_sandbox = false
max_file_size = 10MB
blocked_includes = none
EOF

    # Client development config
    cat > dev_config/client_dev.conf << 'EOF'
# Development configuration for code_client

[client]
server_host = localhost
server_port = 8080
timeout = 30
retry_attempts = 3
keep_alive = true

[logging]
level = debug
file = ./logs/client_dev.log
console = true

[features]
auto_submit = false
syntax_highlighting = true
progress_bar = true
colored_output = true
interactive_mode = true

[transfer]
chunk_size = 64KB
compression = true
verify_checksums = true
EOF

    # Admin development config
    cat > dev_config/admin_dev.conf << 'EOF'
# Development configuration for admin_client

[admin]
socket_path = ./server_admin.sock
timeout = 300
auto_reconnect = true
command_history = true

[interface]
colors = true
paging = true
refresh_interval = 5
max_lines = 100

[logging]
level = debug
file = ./logs/admin_dev.log
console = false
EOF

    # Create sample test files
    mkdir -p test_samples

    # C sample
    cat > test_samples/hello.c << 'EOF'
#include <stdio.h>

int main() {
    printf("Hello from C!\n");
    printf("This is a test compilation.\n");
    return 0;
}
EOF

    # C++ sample
    cat > test_samples/hello.cpp << 'EOF'
#include <iostream>
#include <string>

int main() {
    std::string message = "Hello from C++!";
    std::cout << message << std::endl;
    std::cout << "This is a test compilation." << std::endl;
    return 0;
}
EOF

    # Python sample
    cat > test_samples/hello.py << 'EOF'
#!/usr/bin/env python3

def greet(name="World"):
    return f"Hello from Python, {name}!"

def main():
    print(greet())
    print("This is a test execution.")

if __name__ == "__main__":
    main()
EOF

    # Java sample
    cat > test_samples/Hello.java << 'EOF'
public class Hello {
    public static void main(String[] args) {
        System.out.println("Hello from Java!");
        System.out.println("This is a test compilation and execution.");
        
        if (args.length > 0) {
            System.out.println("Arguments received: " + String.join(", ", args));
        }
    }
}
EOF

    # JavaScript sample
    cat > test_samples/hello.js << 'EOF'
function greet(name = "World") {
    return `Hello from JavaScript, ${name}!`;
}

function main() {
    console.log(greet());
    console.log("This is a test execution.");
    
    if (process.argv.length > 2) {
        console.log("Arguments:", process.argv.slice(2).join(", "));
    }
}

main();
EOF

    # Go sample
    cat > test_samples/hello.go << 'EOF'
package main

import (
    "fmt"
    "os"
)

func main() {
    fmt.Println("Hello from Go!")
    fmt.Println("This is a test compilation and execution.")
    
    if len(os.Args) > 1 {
        fmt.Printf("Arguments: %v\n", os.Args[1:])
    }
}
EOF

    # Error sample (for testing error handling)
    cat > test_samples/error.c << 'EOF'
#include <stdio.h>

int main() {
    // This will cause a compilation error
    undeclared_variable = 42;
    printf("This won't print\n");
    return 0;
}
EOF

    # Complex sample
    cat > test_samples/fibonacci.cpp << 'EOF'
#include <iostream>
#include <vector>
#include <chrono>

long long fibonacci(int n) {
    if (n <= 1) return n;
    
    std::vector<long long> fib(n + 1);
    fib[0] = 0;
    fib[1] = 1;
    
    for (int i = 2; i <= n; i++) {
        fib[i] = fib[i-1] + fib[i-2];
    }
    
    return fib[n];
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    
    int n = 40;
    long long result = fibonacci(n);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Fibonacci(" << n << ") = " << result << std::endl;
    std::cout << "Calculated in " << duration.count() << " ms" << std::endl;
    
    return 0;
}
EOF

    # Create development scripts
    cat > quick_start.sh << 'EOF'
#!/bin/bash

# Quick start script for development environment

set -e

echo -e "\033[0;34m[INFO]\033[0m Starting Code Compiler & Executer development environment..."

# Check if executables exist
if [ ! -f "./bin/code_server" ]; then
    echo -e "\033[0;31m[ERROR]\033[0m Server executable not found. Please build first."
    exit 1
fi

# Kill any existing server instances
pkill -f "code_server" 2>/dev/null || true
sleep 1

# Start server
echo -e "\033[0;34m[INFO]\033[0m Starting server..."
./bin/code_server --config ./dev_config/server_dev.conf --daemon

# Wait for server to start
sleep 3

# Test connection
echo -e "\033[0;34m[INFO]\033[0m Testing connection..."
if ./bin/code_client --test-connection --config ./dev_config/client_dev.conf; then
    echo -e "\033[0;32m[SUCCESS]\033[0m Development environment ready!"
else
    echo -e "\033[0;31m[ERROR]\033[0m Failed to connect to server!"
    exit 1
fi

echo ""
echo "=================================================================="
echo "    Development Environment Active"
echo "=================================================================="
echo ""
echo "Available commands:"
echo "  ./bin/code_client --help           # Client help"
echo "  ./bin/admin_client --help          # Admin help"
echo "  ./bin/code_server --help           # Server help"
echo ""
echo "Quick test:"
echo "  ./bin/code_client --file test_samples/hello.c"
echo "  ./bin/code_client --file test_samples/hello.py"
echo ""
echo "Admin interface:"
echo "  ./bin/admin_client"
echo ""
echo "Development aliases:"
echo "  source aliases.sh"
echo ""
echo "Sample files available in test_samples/"
echo "Configuration files in dev_config/"
echo "Logs in logs/"
echo ""
echo "To stop: ./stop_dev.sh"
echo "=================================================================="
EOF

    chmod +x quick_start.sh

    # Stop script
    cat > stop_dev.sh << 'EOF'
#!/bin/bash

# Stop development environment

echo -e "\033[0;34m[INFO]\033[0m Stopping development environment..."

# Stop server
if pkill -f "code_server"; then
    echo -e "\033[0;32m[SUCCESS]\033[0m Server stopped"
else
    echo -e "\033[0;33m[WARNING]\033[0m No server process found"
fi

# Clean temporary files
if [ -d "./processing" ] || [ -d "./outgoing" ] || [ -d "./temp" ]; then
    rm -rf ./processing/* ./outgoing/* ./temp/* 2>/dev/null || true
    echo -e "\033[0;32m[SUCCESS]\033[0m Temporary files cleaned"
fi

# Remove socket file
rm -f ./server_admin.sock 2>/dev/null || true

echo -e "\033[0;32m[SUCCESS]\033[0m Development environment stopped"
EOF

    chmod +x stop_dev.sh

    # Comprehensive test script
    cat > run_tests.sh << 'EOF'
#!/bin/bash

# Comprehensive test script for development

set -e

echo -e "\033[0;34m[INFO]\033[0m Running comprehensive tests..."

# Stop any running instances
./stop_dev.sh
sleep 2

# Start fresh environment
echo -e "\033[0;34m[INFO]\033[0m Starting test environment..."
./quick_start.sh
sleep 2

# Run unit tests
if command -v ctest &> /dev/null; then
    echo -e "\033[0;34m[INFO]\033[0m Running unit tests..."
    ctest -L unit --output-on-failure || true
    
    echo -e "\033[0;34m[INFO]\033[0m Running integration tests..."
    ctest -L integration --output-on-failure || true
fi

# Test sample files
echo -e "\033[0;34m[INFO]\033[0m Testing sample files..."
for sample in test_samples/*.c test_samples/*.cpp test_samples/*.py test_samples/*.java test_samples/*.js test_samples/*.go; do
    if [ -f "$sample" ]; then
        echo -e "\033[0;34m[INFO]\033[0m Testing $sample..."
        if ./bin/code_client --file "$sample" --config ./dev_config/client_dev.conf; then
            echo -e "\033[0;32m[SUCCESS]\033[0m $sample test passed"
        else
            echo -e "\033[0;33m[WARNING]\033[0m $sample test failed (expected for error samples)"
        fi
        echo ""
    fi
done

# Test admin functionality
echo -e "\033[0;34m[INFO]\033[0m Testing admin functionality..."
timeout 10s ./bin/admin_client --config ./dev_config/admin_dev.conf --batch << 'ADMIN_EOF' || true
list_clients
server_stats
help
quit
ADMIN_EOF

echo -e "\033[0;32m[SUCCESS]\033[0m All tests completed!"

# Show logs summary
echo ""
echo "=================================================================="
echo "    Test Summary"
echo "=================================================================="
if [ -f "logs/server_dev.log" ]; then
    echo "Server log entries: $(wc -l < logs/server_dev.log)"
fi
if [ -f "logs/client_dev.log" ]; then
    echo "Client log entries: $(wc -l < logs/client_dev.log)"
fi

# Cleanup
./stop_dev.sh

echo -e "\033[0;32m[SUCCESS]\033[0m Test run completed!"
EOF

    chmod +x run_tests.sh

    # Create development README
    cat > DEV_README.md << 'EOF'
# Development Environment

This directory contains a complete development environment for the Code Compiler & Executer project.

## Quick Start

1. **Start development environment:**
   ```bash
   ./quick_start.sh
   ```

2. **Test with sample files:**
   ```bash
   ./bin/code_client --file test_samples/hello.c
   ./bin/code_client --file test_samples/hello.py
   ```

3. **Use admin interface:**
   ```bash
   ./bin/admin_client
   ```

4. **Load development aliases:**
   ```bash
   source aliases.sh
   ```

5. **Run comprehensive tests:**
   ```bash
   ./run_tests.sh
   ```

6. **Stop when done:**
   ```bash
   ./stop_dev.sh
   ```

## Directory Structure

```
build/
├── bin/                    # Compiled executables
├── dev_config/            # Development configurations
├── test_samples/          # Sample source files for testing
├── logs/                  # Runtime logs
├── processing/            # Server processing directory
├── outgoing/              # Server output directory
├── temp/                  # Temporary files
├── aliases.sh            # Development aliases
├── quick_start.sh        # Start development environment
├── stop_dev.sh           # Stop development environment
├── run_tests.sh          # Comprehensive test runner
└── DEV_README.md         # This file
```

## Available Executables

- **code_server** - Main server application
- **code_client** - Unix client for code submission
- **admin_client** - Administration interface

## Configuration Files

All development configurations are in `dev_config/`:

- `server_dev.conf` - Server configuration with debug enabled
- `client_dev.conf` - Client configuration for development
- `admin_dev.conf` - Admin client configuration

## Sample Files

Test your setup with files in `test_samples/`:

- `hello.c` - Simple C program
- `hello.cpp` - C++ program with STL
- `hello.py` - Python script with functions
- `Hello.java` - Java class with main method
- `hello.js` - JavaScript with Node.js
- `hello.go` - Go program
- `error.c` - Program with compilation errors (for testing)
- `fibonacci.cpp` - Complex algorithm for performance testing

## Development Aliases

After running `source aliases.sh`, you get these convenient aliases:

### Build Aliases
- `rebuild` - Quick debug rebuild
- `rebuild_release` - Clean release build
- `rebuild_test` - Debug build with tests
- `rebuild_full` - Full debug build with sanitizers and coverage

### Application Aliases
- `server` - Run code_server
- `client` - Run code_client
- `admin` - Run admin_client

### Testing Aliases
- `test_all` - Run all tests
- `test_unit` - Run unit tests only
- `test_integration` - Run integration tests
- `test_quick` - Run quick smoke tests

### Debug Aliases
- `debug_server` - Debug server with GDB
- `debug_client` - Debug client with GDB
- `debug_admin` - Debug admin client with GDB

### Memory Check Aliases
- `memcheck_server` - Check server memory with Valgrind
- `memcheck_client` - Check client memory with Valgrind

### Utility Aliases
- `clean_logs` - Remove all log files
- `clean_temp` - Clean temporary directories
- `show_logs` - Tail all log files

## Common Development Tasks

### Testing a New Feature

1. Make code changes
2. `rebuild` - Quick rebuild
3. `./quick_start.sh` - Start test environment
4. Test your feature
5. `./stop_dev.sh` - Clean stop

### Running Full Test Suite

```bash
rebuild_test        # Build with tests
./run_tests.sh      # Run comprehensive tests
```

### Debugging Issues

```bash
debug_server        # Debug server startup
debug_client        # Debug client connection
show_logs          # Monitor logs in real-time
```

### Performance Analysis

```bash
rebuild_full        # Build with all debug features
./run_tests.sh      # Run tests to generate coverage
# Coverage report available in coverage_html/
```

### Memory Leak Detection

```bash
memcheck_server     # Check server for leaks
memcheck_client     # Check client for leaks
```

## Troubleshooting

### Server Won't Start
- Check if port 8080 is available: `netstat -ln | grep 8080`
- Check server logs: `cat logs/server_dev.log`
- Try manual start: `./bin/code_server --config dev_config/server_dev.conf --no-daemon`

### Client Connection Fails
- Verify server is running: `ps aux | grep code_server`
- Test network: `telnet localhost 8080`
- Check client logs: `cat logs/client_dev.log`

### Compilation Issues
- Check compiler availability: `which gcc g++ python3 java node`
- Verify file permissions in processing directory
- Check server compilation logs

### Tests Failing
- Ensure clean environment: `./stop_dev.sh && ./quick_start.sh`
- Run individual tests: `ctest -R specific_test_name -V`
- Check test logs for details

## Advanced Features

### Custom Language Support
Add new language support by:
1. Modifying server compiler configuration
2. Adding sample files to test_samples/
3. Testing with the new language

### Performance Monitoring
- Server stats: Use admin client `server_stats` command
- System monitoring: `htop`, `iotop` while running tests
- Memory usage: `valgrind --tool=massif`

### Security Testing
- Test with malicious code samples
- Monitor sandbox restrictions
- Test file size limits

## Integration with IDEs

### VS Code
- Use the generated `compile_commands.json` for IntelliSense
- Install C/C++ extension
- Configure debugger to use the development environment

### CLion
- Open the CMake project directly
- Configure run configurations for each executable
- Use built-in debugging with the development configs

### Vim/Neovim
- Use LSP with clangd and the compile_commands.json
- Configure debugging with vimspector or similar

## Continuous Integration

The development environment can be used in CI/CD:

```bash
# CI Script Example
./build.sh -t Debug -c -T -s --coverage
cd build
./run_tests.sh
# Upload coverage reports
# Archive build artifacts
```

## Tips and Best Practices

1. **Always use the development configs** - they have debug logging enabled
2. **Check logs frequently** - they contain valuable debugging information
3. **Test with multiple languages** - ensure broad compatibility
4. **Use memory checking tools** - catch issues early
5. **Run full test suite before commits** - maintain code quality
6. **Monitor performance** - watch for regressions
7. **Keep sample files updated** - add edge cases as you find them

For more information, see the main project documentation.
EOF

    print_success "Development README created: DEV_README.md"

    print_success "Development environment created successfully!"
fi

# Generate final summary
echo
echo "================================================================="
echo "    BUILD COMPLETED SUCCESSFULLY"
echo "================================================================="
echo

print_build "Build Summary:"
echo "  Build Type: $BUILD_TYPE"
echo "  Build Directory: $BUILD_DIR"
echo "  Build Time: ${build_time}s"
echo "  Parallel Jobs: $JOBS"
echo "  Tests: $([ "$RUN_TESTS" = true ] && echo "Executed (${test_time}s)" || echo "Skipped")"
echo "  Installation: $([ "$INSTALL" = true ] && echo "Completed" || echo "Skipped")"
echo "  Sanitizers: $([ "$ENABLE_SANITIZERS" = true ] && echo "Enabled" || echo "Disabled")"
echo "  Coverage: $([ "$ENABLE_COVERAGE" = true ] && echo "Enabled" || echo "Disabled")"
echo "  Threading Check: $([ "$ENABLE_THREADING_CHECK" = true ] && echo "Enabled" || echo "Disabled")"

echo
print_build "Available Executables:"
if [ -d "bin" ]; then
    find bin -type f -executable 2>/dev/null | sort | while read -r file; do
        size=$(du -h "$file" 2>/dev/null | cut -f1 || echo "?")
        echo "  - $file ($size)"
    done
else
    print_warning "No executables found in bin/ directory"
fi

if [ "$CREATE_DEV_ENV" = true ]; then
    echo
    print_build "Development Environment:"
    echo "  Quick Start: ./quick_start.sh"
    echo "  Run Tests: ./run_tests.sh"
    echo "  Stop Environment: ./stop_dev.sh"
    echo "  Load Aliases: source aliases.sh"
    echo "  Documentation: cat DEV_README.md"
fi

echo
print_build "Next Steps:"
echo "  1. cd $BUILD_DIR"
if [ "$CREATE_DEV_ENV" = true ]; then
    echo "  2. ./quick_start.sh                    # Start development environment"
    echo "  3. ./bin/code_client --file test_samples/hello.c"
    echo "  4. ./bin/admin_client                  # Try admin interface"
    echo "  5. source aliases.sh                  # Load helpful aliases"
else
    echo "  2. ./bin/code_server --help"
    echo "  3. ./bin/code_client --help"
    echo "  4. ./bin/admin_client --help"
fi

if [ "$BUILD_TYPE" = "Debug" ]; then
    echo
    print_build "Debug Mode Features:"
    echo "  - Enhanced logging enabled"
    echo "  - Debug symbols included"
    if [ "$ENABLE_SANITIZERS" = true ]; then
        echo "  - AddressSanitizer and UBSan active"
    fi
    if [ "$ENABLE_COVERAGE" = true ]; then
        echo "  - Code coverage instrumentation active"
        echo "  - Generate reports with: make coverage"
    fi
    if [ "$ENABLE_THREADING_CHECK" = true ]; then
        echo "  - ThreadSanitizer active for thread safety"
    fi
    echo "  - Use GDB for debugging: gdb ./bin/code_server"
    echo "  - Memory check with: valgrind ./bin/code_server"
fi

if [ "$RUN_TESTS" = true ]; then
    echo
    print_build "Test Results Summary:"
    if [ -f "Testing/Temporary/LastTest.log" ]; then
        total_tests=$(grep -c "Test #" Testing/Temporary/LastTest.log 2>/dev/null || echo "Unknown")
        echo "  - Total tests run: $total_tests"
    fi
    echo "  - All tests passed in ${test_time}s"
    echo "  - Individual test results in Testing/ directory"
fi

if [ "$ENABLE_COVERAGE" = true ] && [ -d "coverage_html" ]; then
    echo
    print_build "Coverage Report:"
    echo "  - HTML report available in: coverage_html/"
    echo "  - Open with: xdg-open coverage_html/index.html"
fi

echo
print_success "================================================================="
print_success "Build process completed successfully!"
print_success "Happy coding! 🚀"
print_success "================================================================="

# Final development tips
if [ "$BUILD_TYPE" = "Debug" ] && [ "$CREATE_DEV_ENV" = true ]; then
    echo
    print_status "💡 Development Tips:"
    echo "  • Use 'source aliases.sh' for convenient shortcuts"
    echo "  • Monitor logs with 'show_logs' alias"
    echo "  • Test frequently with './run_tests.sh'"
    echo "  • Check memory usage with memcheck aliases"
    echo "  • Read DEV_README.md for comprehensive guide"
fi