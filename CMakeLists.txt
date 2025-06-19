cmake_minimum_required(VERSION 3.16)
project(CodeCompilerExecuter 
        VERSION 1.0.0 
        DESCRIPTION "Code Compiler & Executer - Client-Server Application"
        LANGUAGES C CXX)

# Standard C/C++
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Opțiuni de compilare pentru toate targeturile
set(COMMON_C_FLAGS "-Wall -Wextra -Wpedantic")
set(COMMON_CXX_FLAGS "-Wall -Wextra -Wpedantic")

# Configurarea tipurilor de build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Build type not specified: defaulting to Release")
endif()

# Flag-uri pentru Debug/Release
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Aplicarea flag-urilor comune
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_CXX_FLAGS}")

# Găsirea bibliotecilor necesare
find_package(Threads REQUIRED)

# Opțiuni pentru features avansate
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_ASAN "Enable AddressSanitizer (Debug only)" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer (Debug only)" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (Debug only)" OFF)
option(BUILD_WITH_NCURSES "Build admin client with ncurses support" ON)

# Configurare sanitizers pentru debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(ENABLE_ASAN)
        message(STATUS "Enabling AddressSanitizer")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
    endif()
    
    if(ENABLE_TSAN)
        message(STATUS "Enabling ThreadSanitizer")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=thread")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")
    endif()
    
    if(ENABLE_UBSAN)
        message(STATUS "Enabling UndefinedBehaviorSanitizer")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=undefined")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined")
    endif()
endif()

# Verificări platform-specific
if(UNIX AND NOT APPLE)
    add_definitions(-D_GNU_SOURCE)
    # Verificare pentru funcții Linux-specific
    include(CheckFunctionExists)
    check_function_exists(inotify_init HAVE_INOTIFY)
    if(HAVE_INOTIFY)
        add_definitions(-DHAVE_INOTIFY=1)
        message(STATUS "inotify support detected")
    endif()
endif()

# Configurare directoare de output
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Includere subdirectoare în ordinea dependințelor
add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(admin_client)
add_subdirectory(client_unix)

# Testing (opțional)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Test suite enabled")
endif()

# Configurare instalare
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE PATH "Installation prefix")

# Crearea directoarelor de runtime la instalare
install(DIRECTORY DESTINATION var/log/code_server)
install(DIRECTORY DESTINATION var/lib/code_server/processing)
install(DIRECTORY DESTINATION var/lib/code_server/outgoing)
install(DIRECTORY DESTINATION etc/code_server)

# Fișiere de configurare
configure_file(
    ${CMAKE_SOURCE_DIR}/docs/server.conf.example
    ${CMAKE_BINARY_DIR}/etc/server.conf.example
    COPYONLY
)

# Target pentru curățare completă
add_custom_target(distclean
    COMMAND ${CMAKE_BUILD_TOOL} clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/install
)

# Target pentru formatarea codului (dacă ai clang-format)
find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/server/src/*.c
        ${CMAKE_SOURCE_DIR}/server/include/*.h
        ${CMAKE_SOURCE_DIR}/admin_client/src/*.cpp
        ${CMAKE_SOURCE_DIR}/admin_client/include/*.h
        ${CMAKE_SOURCE_DIR}/client_unix/src/*.cpp
        ${CMAKE_SOURCE_DIR}/client_unix/include/*.h
        ${CMAKE_SOURCE_DIR}/common/*.h
    )
    
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting source code with clang-format"
    )
endif()

# Afișare informații de build
message(STATUS "")
message(STATUS "=== Code Compiler & Executer Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build tests: ${BUILD_TESTS}")
if(BUILD_WITH_NCURSES)
    message(STATUS "ncurses support: Enabled")
endif()
message(STATUS "===================================================")
message(STATUS "")