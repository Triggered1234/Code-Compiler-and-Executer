cmake_minimum_required(VERSION 3.10)

# Project definition
project(CodeCompilerExecutor 
    VERSION 1.0.0
    DESCRIPTION "Code Compiler and Executor Server"
    LANGUAGES C CXX)

# Set C and C++ standards
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS_DEBUG "-g -Wall -Wextra -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -Wall -Wextra -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wextra -DNDEBUG")

# Find required packages
find_package(Threads REQUIRED)

# Optional: Find Doxygen for documentation
find_package(Doxygen QUIET)

# Check for required libraries
include(CheckLibraryExists)
check_library_exists(pthread pthread_create "" HAVE_PTHREAD)

if(NOT HAVE_PTHREAD)
    message(FATAL_ERROR "pthread library not found")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Create build directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/processing)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/outgoing)

# Subdirectories
add_subdirectory(src)

# Custom targets for running applications
add_custom_target(run-server
    COMMAND ${CMAKE_BINARY_DIR}/src/server
    DEPENDS server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running the server application"
)

add_custom_target(run-admin
    COMMAND ${CMAKE_BINARY_DIR}/src/admin_client
    DEPENDS admin_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running the admin client"
)

add_custom_target(run-client
    COMMAND ${CMAKE_BINARY_DIR}/src/client
    DEPENDS client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running the regular client"
)

add_custom_target(run-python-client
    COMMAND python3 ${CMAKE_SOURCE_DIR}/src/client.py
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running the Python client"
)

# Install targets
install(TARGETS server admin_client client
    RUNTIME DESTINATION bin
)

install(FILES src/client.py
    DESTINATION bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)

# Create directories on install
install(DIRECTORY DESTINATION processing)
install(DIRECTORY DESTINATION outgoing)

# Doxygen documentation
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
    
    add_custom_target(docs-clean
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/docs
        COMMENT "Cleaning documentation directory"
    )
    
    message(STATUS "Doxygen found: Documentation can be built with 'make docs'")
else()
    message(STATUS "Doxygen not found: Documentation target not available")
endif()

# Help target
add_custom_target(show-help
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Code Compiler & Executor - CMake Build"
    COMMAND ${CMAKE_COMMAND} -E echo "====================================="
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Build targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  all              - Build all components"
    COMMAND ${CMAKE_COMMAND} -E echo "  server           - Build server only"
    COMMAND ${CMAKE_COMMAND} -E echo "  admin_client     - Build admin client only"
    COMMAND ${CMAKE_COMMAND} -E echo "  client           - Build regular client only"
    COMMAND ${CMAKE_COMMAND} -E echo "  install          - Install all components"
    COMMAND ${CMAKE_COMMAND} -E echo "  docs             - Generate Doxygen documentation"
    COMMAND ${CMAKE_COMMAND} -E echo "  docs-clean       - Clean documentation directory"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Run targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  run-server       - Start the server"
    COMMAND ${CMAKE_COMMAND} -E echo "  run-admin        - Start admin client"
    COMMAND ${CMAKE_COMMAND} -E echo "  run-client       - Start regular client"
    COMMAND ${CMAKE_COMMAND} -E echo "  run-python-client- Start Python client"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Usage example:"
    COMMAND ${CMAKE_COMMAND} -E echo "  mkdir build && cd build"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake .."
    COMMAND ${CMAKE_COMMAND} -E echo "  make"
    COMMAND ${CMAKE_COMMAND} -E echo "  make run-server    # In terminal 1"
    COMMAND ${CMAKE_COMMAND} -E echo "  make run-admin     # In terminal 2"
    COMMAND ${CMAKE_COMMAND} -E echo "  make run-client    # In terminal 3"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)