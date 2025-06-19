#!/bin/bash

# Build script for Code Compiler & Executor
# This script provides easy building and running of the project

set -e  # Exit on any error

PROJECT_NAME="Code Compiler & Executor"
BUILD_DIR="build"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_usage() {
    echo -e "${BLUE}$PROJECT_NAME - Build Script${NC}"
    echo "================================"
    echo ""
    echo "Usage: $0 [OPTION]"
    echo ""
    echo "Options:"
    echo "  build             Build the project"
    echo "  clean             Clean build directory"
    echo "  rebuild           Clean and build"
    echo "  install           Build and install"
    echo "  run-server        Build and run server"
    echo "  run-admin         Build and run admin client"
    echo "  run-client        Build and run regular client"
    echo "  run-python        Run Python client"
    echo "  docs              Generate Doxygen documentation"
    echo "  docs-clean        Clean documentation"
    echo "  help              Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 build          # Build all components"
    echo "  $0 run-server     # Build and start server"
}

check_dependencies() {
    echo -e "${YELLOW}Checking dependencies...${NC}"
    
    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        echo -e "${RED}Error: CMake is not installed${NC}"
        echo "Please install CMake version 3.10 or higher"
        exit 1
    fi
    
    # Check for GCC/G++
    if ! command -v gcc &> /dev/null || ! command -v g++ &> /dev/null; then
        echo -e "${RED}Error: GCC/G++ not found${NC}"
        echo "Please install build-essential package"
        exit 1
    fi
    
    # Check for Python3 (optional)
    if ! command -v python3 &> /dev/null; then
        echo -e "${YELLOW}Warning: Python3 not found. Python client will not work.${NC}"
    fi
    
    # Check for Doxygen (optional)
    if ! command -v doxygen &> /dev/null; then
        echo -e "${YELLOW}Warning: Doxygen not found. Documentation generation will not work.${NC}"
    fi
    
    echo -e "${GREEN}Dependencies check passed${NC}"
}

build_project() {
    echo -e "${YELLOW}Building $PROJECT_NAME...${NC}"
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with CMake
    echo -e "${YELLOW}Configuring with CMake...${NC}"
    cmake .. -DCMAKE_BUILD_TYPE=Release
    
    # Build
    echo -e "${YELLOW}Compiling...${NC}"
    make -j$(nproc 2>/dev/null || echo 4)
    
    echo -e "${GREEN}Build completed successfully!${NC}"
    echo ""
    echo "Executables created:"
    echo "  - bin/server         (Server application)"
    echo "  - bin/admin_client   (Admin client)"
    echo "  - bin/client         (Regular client)"
    echo "  - bin/client.py      (Python client)"
    
    cd "$SOURCE_DIR"
}

generate_docs() {
    if [ ! -d "$BUILD_DIR" ]; then
        echo -e "${YELLOW}Build directory not found, building first...${NC}"
        build_project
    fi
    
    echo -e "${YELLOW}Generating documentation...${NC}"
    cd "$BUILD_DIR"
    
    if command -v doxygen &> /dev/null; then
        make docs
        echo -e "${GREEN}Documentation generated in docs/html/index.html${NC}"
    else
        echo -e "${RED}Error: Doxygen not installed${NC}"
        echo "Please install Doxygen: sudo apt-get install doxygen"
        cd "$SOURCE_DIR"
        return 1
    fi
    
    cd "$SOURCE_DIR"
}

clean_docs() {
    echo -e "${YELLOW}Cleaning documentation...${NC}"
    if [ -d "$BUILD_DIR" ]; then
        cd "$BUILD_DIR"
        make docs-clean 2>/dev/null || true
        cd "$SOURCE_DIR"
    fi
    rm -rf docs/
    echo -e "${GREEN}Documentation cleaned${NC}"
}

clean_project() {
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}Clean completed${NC}"
}

run_server() {
    if [ ! -f "$BUILD_DIR/bin/server" ]; then
        echo -e "${YELLOW}Server not built, building first...${NC}"
        build_project
    fi
    
    echo -e "${GREEN}Starting server...${NC}"
    cd "$BUILD_DIR"
    ./bin/server
}

run_admin() {
    if [ ! -f "$BUILD_DIR/bin/admin_client" ]; then
        echo -e "${YELLOW}Admin client not built, building first...${NC}"
        build_project
    fi
    
    echo -e "${GREEN}Starting admin client...${NC}"
    cd "$BUILD_DIR"
    ./bin/admin_client
}

run_client() {
    if [ ! -f "$BUILD_DIR/bin/client" ]; then
        echo -e "${YELLOW}Client not built, building first...${NC}"
        build_project
    fi
    
    echo -e "${GREEN}Starting regular client...${NC}"
    cd "$BUILD_DIR"
    ./bin/client
}

run_python() {
    if [ ! -f "$BUILD_DIR/bin/client.py" ]; then
        echo -e "${YELLOW}Python client not found, building first...${NC}"
        build_project
    fi
    
    echo -e "${GREEN}Starting Python client...${NC}"
    cd "$BUILD_DIR"
    python3 bin/client.py
}

install_project() {
    if [ ! -d "$BUILD_DIR" ]; then
        build_project
    fi
    
    echo -e "${YELLOW}Installing project...${NC}"
    cd "$BUILD_DIR"
    sudo make install
    echo -e "${GREEN}Installation completed${NC}"
    cd "$SOURCE_DIR"
}

# Main script logic
case "$1" in
    "build")
        check_dependencies
        build_project
        ;;
    "clean")
        clean_project
        ;;
    "rebuild")
        clean_project
        check_dependencies
        build_project
        ;;
    "install")
        check_dependencies
        build_project
        install_project
        ;;
    "run-server")
        check_dependencies
        run_server
        ;;
    "run-admin")
        check_dependencies
        run_admin
        ;;
    "run-client")
        check_dependencies
        run_client
        ;;
    "run-python")
        run_python
        ;;
    "docs")
        generate_docs
        ;;
    "docs-clean")
        clean_docs
        ;;
    "help"|"--help"|"-h")
        print_usage
        ;;
    "")
        print_usage
        ;;
    *)
        echo -e "${RED}Error: Unknown option '$1'${NC}"
        echo ""
        print_usage
        exit 1
        ;;
esac