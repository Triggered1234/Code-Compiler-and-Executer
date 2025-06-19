#!/bin/bash

# Setup script pentru proiectul Code Compiler & Executer
# Prima configurare a mediului de development

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

print_header() {
    echo -e "${CYAN}"
    echo "================================================================="
    echo "    Code Compiler & Executer - Project Setup"
    echo "    Rares-Nicholas Popa & Adrian-Petru Enache"
    echo "================================================================="
    echo -e "${NC}"
}

print_status() {
    echo -e "${BLUE}[SETUP]${NC} $1"
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

print_info() {
    echo -e "${PURPLE}[INFO]${NC} $1"
}

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if [ -f /etc/debian_version ]; then
            OS="debian"
            DISTRO=$(lsb_release -si 2>/dev/null || echo "Debian")
        elif [ -f /etc/redhat-release ]; then
            OS="redhat"
            DISTRO=$(cat /etc/redhat-release | cut -d' ' -f1)
        else
            OS="linux"
            DISTRO="Unknown"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
        DISTRO="macOS"
    else
        OS="unknown"
        DISTRO="Unknown"
    fi
}

# Install dependencies based on OS
install_dependencies() {
    print_status "Installing dependencies for $DISTRO..."
    
    case $OS in
        "debian")
            sudo apt-get update
            
            # Essential build tools
            sudo apt-get install -y \
                build-essential \
                cmake \
                git \
                pkg-config
            
            # Development libraries
            sudo apt-get install -y \
                libncurses5-dev \
                libncursesw5-dev \
                libreadline-dev \
                libjson-c-dev \
                libssl-dev \
                zlib1g-dev \
                uuid-dev
            
            # Testing framework
            sudo apt-get install -y \
                libgtest-dev \
                libgmock-dev
            
            # Runtime compilers
            sudo apt-get install -y \
                gcc \
                g++ \
                python3 \
                python3-pip \
                default-jdk \
                nodejs \
                npm
            
            # Optional: Go and Rust
            if command -v snap &> /dev/null; then
                sudo snap install go --classic || print_warning "Failed to install Go via snap"
                sudo snap install rustup --classic || print_warning "Failed to install Rust via snap"
            fi
            
            # Development tools
            sudo apt-get install -y \
                valgrind \
                gdb \
                lcov \
                clang-format \
                doxygen
            ;;
            
        "redhat")
            # Update package manager
            if command -v dnf &> /dev/null; then
                PKG_MGR="dnf"
            else
                PKG_MGR="yum"
            fi
            
            sudo $PKG_MGR update -y
            
            # Essential build tools
            sudo $PKG_MGR install -y \
                gcc \
                gcc-c++ \
                cmake \
                git \
                pkgconfig
            
            # Development libraries
            sudo $PKG_MGR install -y \
                ncurses-devel \
                readline-devel \
                json-c-devel \
                openssl-devel \
                zlib-devel \
                libuuid-devel
            
            # Testing framework
            sudo $PKG_MGR install -y \
                gtest-devel \
                gmock-devel
            
            # Runtime compilers
            sudo $PKG_MGR install -y \
                python3 \
                python3-pip \
                java-11-openjdk-devel \
                nodejs \
                npm
            
            # Development tools
            sudo $PKG_MGR install -y \
                valgrind \
                gdb \
                lcov
            ;;
            
        "macos")
            # Check for Homebrew
            if ! command -v brew &> /dev/null; then
                print_error "Homebrew not found. Please install Homebrew first:"
                print_error "https://brew.sh"
                exit 1
            fi
            
            # Update Homebrew
            brew update
            
            # Essential build tools
            brew install cmake git pkg-config
            
            # Development libraries
            brew install ncurses readline json-c openssl zlib
            
            # Testing framework
            brew install googletest
            
            # Runtime compilers
            brew install python3 openjdk node go rust
            
            # Development tools
            brew install valgrind gdb lcov clang-format doxygen
            ;;
            
        *)
            print_error "Unsupported operating system: $OSTYPE"
            print_error "Please install dependencies manually:"
            print_error "- CMake (>= 3.16)"
            print_error "- GCC/G++ compiler"
            print_error "- Development libraries (ncurses, readline, json-c, openssl)"
            print_error "- Google Test framework"
            print_error "- Runtime compilers (gcc, g++, python3, java, node)"
            exit 1
            ;;
    esac
    
    print_success "Dependencies installed successfully"
}

# Setup Git hooks
setup_git_hooks() {
    print_status "Setting up Git hooks..."
    
    mkdir -p .git/hooks
    
    # Pre-commit hook
    cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
# Pre-commit hook for Code Compiler & Executer

echo "Running pre-commit checks..."

# Check for debug prints in code
if git diff --cached --name-only | grep -E '\.(c|cpp|h)$' | xargs grep -l "printf.*DEBUG\|cout.*DEBUG" 2>/dev/null; then
    echo "ERROR: Debug prints found in staged files"
    echo "Please remove debug prints before committing"
    exit 1
fi

# Format code if clang-format is available
if command -v clang-format &> /dev/null; then
    echo "Formatting staged files..."
    git diff --cached --name-only | grep -E '\.(c|cpp|h)$' | while read file; do
        if [ -f "$file" ]; then
            clang-format -i "$file"
            git add "$file"
        fi
    done
fi

# Run quick tests if in build directory
if [ -d "build" ] && [ -f "build/bin/code_server" ]; then
    echo "Running quick tests..."
    cd build
    if ! ctest -R "protocol_tests|queue_tests" --output-on-failure; then
        echo "ERROR: Quick tests failed"
        echo "Please fix tests before committing"
        exit 1
    fi
    cd ..
fi

echo "Pre-commit checks passed"
EOF

    chmod +x .git/hooks/pre-commit
    
    # Pre-push hook
    cat > .git/hooks/pre-push << 'EOF'
#!/bin/bash
# Pre-push hook for Code Compiler & Executer

echo "Running pre-push checks..."

# Run full test suite if available
if [ -d "build" ] && [ -f "build/run_tests.sh" ]; then
    echo "Running full test suite..."
    cd build
    if ! ./run_tests.sh; then
        echo "ERROR: Test suite failed"
        echo "Please fix all tests before pushing"
        read -p "Push anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
    cd ..
fi

echo "Pre-push checks passed"
EOF

    chmod +x .git/hooks/pre-push
    
    print_success "Git hooks configured"
}

# Create development workspace
create_workspace() {
    print_status "Creating development workspace..."
    
    # Create common directories
    mkdir -p {docs,scripts,configs,samples}
    
    # Create .gitignore if it doesn't exist
    if [ ! -f .gitignore ]; then
        cat > .gitignore << 'EOF'
# Build directories
build/
install/
.cmake/

# IDE files
.vscode/
.idea/
*.swp
*.swo
*~

# Compiled objects
*.o
*.so
*.a
*.dll
*.exe

# Logs
*.log
logs/

# Temporary files
tmp/
temp/
*.tmp

# OS specific
.DS_Store
Thumbs.db

# Runtime directories
processing/
outgoing/

# Coverage reports
coverage_html/
coverage.info
*.gcov
*.gcda
*.gcno

# Valgrind output
vgcore.*
*.memcheck

# Editor backup files
*~
.#*
#*#

# Package files
*.deb
*.rpm
*.tar.gz

# Python cache
__pycache__/
*.pyc
*.pyo

# Node modules (if using npm for any tools)
node_modules/

# Local configuration
local.conf
*.local
EOF
    fi
    
    # Create EditorConfig
    cat > .editorconfig << 'EOF'
# EditorConfig for Code Compiler & Executer

root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true
indent_style = space
indent_size = 4

[*.{c,h}]
indent_size = 4

[*.{cpp,hpp}]
indent_size = 4

[*.py]
indent_size = 4

[*.js]
indent_size = 2

[*.java]
indent_size = 4

[*.go]
indent_style = tab

[*.md]
trim_trailing_whitespace = false

[CMakeLists.txt]
indent_size = 4

[*.cmake]
indent_size = 4

[*.yml,*.yaml]
indent_size = 2

[*.json]
indent_size = 2
EOF

    # Create clang-format configuration
    cat > .clang-format << 'EOF'
---
Language: Cpp
BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100
BreakBeforeBraces: Linux
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
AllowShortFunctionsOnASingleLine: None
IndentCaseLabels: true
SpacesInParentheses: false
SpacesInSquareBrackets: false
SpaceAfterCStyleCast: true
PointerAlignment: Left
AlignTrailingComments: true
SortIncludes: true
EOF
    
    print_success "Development workspace created"
}

# Create documentation structure
create_docs() {
    print_status "Creating documentation structure..."
    
    # Main README
    if [ ! -f README.md ]; then
        cat > README.md << 'EOF'
# Code Compiler & Executer

A client-server application for remote code compilation and execution.

## Team
- Rares-Nicholas Popa
- Adrian-Petru Enache

## Project Overview

This project implements a client-server system that allows users to submit source code files to a server for compilation and execution. The system supports multiple programming languages and provides both administrative and regular client interfaces.

## Quick Start

1. **Setup environment:**
   ```bash
   ./setup.sh
   ```

2. **Build project:**
   ```bash
   ./build.sh -t Debug -c -T
   ```

3. **Start development environment:**
   ```bash
   cd build
   ./quick_start.sh
   ```

4. **Test with sample code:**
   ```bash
   ./bin/code_client --file test_samples/hello.c
   ```

## Architecture

- **Server (C)**: Main compilation and execution engine
- **Admin Client (C++)**: Administrative interface with UNIX socket
- **Unix Client (C++)**: Regular client for UNIX/Linux systems  
- **Python Client**: Cross-platform client implementation

## Features

- Multi-language support (C, C++, Python, Java, JavaScript, Go)
- Secure compilation environment
- Real-time job monitoring
- Administrative controls
- Async and sync communication modes
- File transfer capabilities

## Documentation

- [Build Instructions](docs/BUILD.md)
- [API Documentation](docs/API.md)
- [User Manual](docs/USER_MANUAL.md)
- [Development Guide](docs/DEVELOPMENT.md)

## License

This project is developed for educational purposes as part of the PCD course.
EOF
    fi
    
    # API documentation template
    cat > docs/API.md << 'EOF'
# API Documentation

## Protocol Overview

The system uses a custom binary protocol over TCP sockets for client-server communication and UNIX sockets for administration.

## Message Format

All messages follow this structure:
- Magic number (4 bytes)
- Message type (2 bytes)  
- Flags (2 bytes)
- Data length (4 bytes)
- Correlation ID (4 bytes)
- Timestamp (8 bytes)
- Checksum (4 bytes)
- Data payload (variable)

## Message Types

### Client Messages
- `MSG_COMPILE_REQUEST` - Submit code for compilation
- `MSG_STATUS_REQUEST` - Check job status
- `MSG_RESULT_REQUEST` - Retrieve results

### Server Responses
- `MSG_COMPILE_RESPONSE` - Compilation results
- `MSG_STATUS_RESPONSE` - Job status update
- `MSG_ERROR` - Error information

### Admin Messages
- `MSG_ADMIN_LIST_CLIENTS` - List connected clients
- `MSG_ADMIN_LIST_JOBS` - List active jobs
- `MSG_ADMIN_SERVER_STATS` - Get server statistics

## Examples

See the client implementations for usage examples.
EOF

    # Build instructions
    cat > docs/BUILD.md << 'EOF'
# Build Instructions

## Prerequisites

- CMake 3.16+
- GCC/G++ compiler
- Development libraries (see setup.sh)

## Build Types

### Release Build
```bash
./build.sh
```

### Debug Build with Tests
```bash
./build.sh -t Debug -c -T
```

### Full Development Build
```bash
./build.sh -t Debug -c -T -s --coverage
```

## Advanced Options

- `-s, --sanitizers`: Enable AddressSanitizer and UBSan
- `--coverage`: Enable code coverage analysis
- `--threading`: Enable ThreadSanitizer
- `-j N`: Use N parallel jobs

## Cross-platform Notes

The build system supports Linux, macOS, and can be adapted for Windows with MinGW.
EOF

    print_success "Documentation structure created"
}

# Verify installation
verify_installation() {
    print_status "Verifying installation..."
    
    # Check build tools
    TOOLS_OK=true
    
    check_tool() {
        if command -v "$1" &> /dev/null; then
            print_success "$1 available"
        else
            print_error "$1 not found"
            TOOLS_OK=false
        fi
    }
    
    check_tool "cmake"
    check_tool "make"
    check_tool "gcc"
    check_tool "g++"
    
    # Check optional tools
    check_tool "python3"
    check_tool "java" || print_warning "Java not available - Java compilation disabled"
    check_tool "node" || print_warning "Node.js not available - JavaScript execution disabled"
    
    if [ "$TOOLS_OK" = false ]; then
        print_error "Some required tools are missing"
        return 1
    fi
    
    print_success "Installation verification completed"
    return 0
}

# Main setup function
main() {
    print_header
    
    print_info "This script will set up the development environment for"
    print_info "the Code Compiler & Executer project."
    echo
    
    # Detect operating system
    detect_os
    print_status "Detected OS: $DISTRO ($OS)"
    
    # Ask for confirmation
    read -p "Do you want to install dependencies? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        install_dependencies
    else
        print_warning "Skipping dependency installation"
        print_warning "Make sure you have all required dependencies installed"
    fi
    
    # Setup Git hooks
    if [ -d ".git" ]; then
        read -p "Setup Git hooks? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            setup_git_hooks
        fi
    else
        print_warning "Not a Git repository - skipping Git hooks"
    fi
    
    # Create workspace
    create_workspace
    
    # Create documentation
    create_docs
    
    # Verify installation
    if verify_installation; then
        print_success "Setup completed successfully!"
        
        echo
        print_info "Next steps:"
        echo "  1. Build the project:     ./build.sh -t Debug -c -T"
        echo "  2. Start development:     cd build && ./quick_start.sh"
        echo "  3. Read documentation:    cat docs/BUILD.md"
        echo "  4. Load dev aliases:      source build/aliases.sh"
        echo
        print_info "For help: ./build.sh --help"
        
    else
        print_error "Setup completed with errors"
        print_error "Please check the error messages above and fix any issues"
        exit 1
    fi
}

# Run main function
main "$@"