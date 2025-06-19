# Source directory CMakeLists.txt

# Server executable
add_executable(server
    server.c
)

target_link_libraries(server
    Threads::Threads
)

# Admin client executable
add_executable(admin_client
    admin_client.cpp
)

target_link_libraries(admin_client
    Threads::Threads
)

# Regular client executable
add_executable(client
    client.cpp
)

target_link_libraries(client
    Threads::Threads
)

# Set output directory for executables
set_target_properties(server admin_client client
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Copy Python client to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/client.py
    ${CMAKE_BINARY_DIR}/bin/client.py
    COPYONLY
)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    # Linux specific settings
    target_compile_definitions(server PRIVATE _GNU_SOURCE)
    target_compile_definitions(admin_client PRIVATE _GNU_SOURCE)
    target_compile_definitions(client PRIVATE _GNU_SOURCE)
elseif(APPLE)
    # macOS specific settings
    target_compile_definitions(server PRIVATE _DARWIN_C_SOURCE)
    target_compile_definitions(admin_client PRIVATE _DARWIN_C_SOURCE)
    target_compile_definitions(client PRIVATE _DARWIN_C_SOURCE)
endif()

# Compiler-specific warnings
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(server PRIVATE
        -Wall -Wextra -Wpedantic
        -Wformat=2 -Wstrict-prototypes
        -Wmissing-prototypes -Wold-style-definition
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(admin_client PRIVATE
        -Wall -Wextra -Wpedantic
        -Wformat=2 -Woverloaded-virtual
    )
    target_compile_options(client PRIVATE
        -Wall -Wextra -Wpedantic
        -Wformat=2 -Woverloaded-virtual
    )
endif()