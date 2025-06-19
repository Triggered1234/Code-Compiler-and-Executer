# server/CMakeLists.txt
# Aplicația server pentru compilarea și execuția codului

# Lista fișierelor sursă
set(SERVER_SOURCES
    src/server.c
    src/admin_handler.c
    src/client_handler.c
    src/compiler_service.c
    src/file_manager.c
    src/queue_manager.c
)

# Lista header-urilor (locale în server/include)
set(SERVER_HEADERS
    include/server.h
    include/common.h
    include/compiler.h
)

# Crearea executabilului server
add_executable(code_server ${SERVER_SOURCES} ${SERVER_HEADERS})

# Alias pentru claritate
add_executable(CodeCompiler::server ALIAS code_server)

# Configurarea directoarelor include
target_include_directories(code_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Linkarea cu biblioteca comună
target_link_libraries(code_server PRIVATE
    CodeCompiler::common
)

# Definirea macro-urilor specifice serverului
target_compile_definitions(code_server PRIVATE
    SERVER_VERSION="${PROJECT_VERSION}"
    DEFAULT_LOG_DIR="/var/log/code_server"
    DEFAULT_PROCESSING_DIR="/var/lib/code_server/processing"
    DEFAULT_OUTGOING_DIR="/var/lib/code_server/outgoing"
    DEFAULT_CONFIG_FILE="/etc/code_server/server.conf"
)

# Platform-specific configurations
if(UNIX AND NOT APPLE)
    # Linux specific features
    target_compile_definitions(code_server PRIVATE LINUX_PLATFORM=1)
    
    # Check for advanced Linux features
    include(CheckIncludeFile)
    check_include_file(sys/inotify.h HAVE_SYS_INOTIFY_H)
    check_include_file(sys/epoll.h HAVE_SYS_EPOLL_H)
    check_include_file(sys/signalfd.h HAVE_SYS_SIGNALFD_H)
    
    if(HAVE_SYS_INOTIFY_H)
        target_compile_definitions(code_server PRIVATE HAVE_INOTIFY=1)
        message(STATUS "Server: inotify support enabled")
    endif()
    
    if(HAVE_SYS_EPOLL_H)
        target_compile_definitions(code_server PRIVATE HAVE_EPOLL=1)
        message(STATUS "Server: epoll support enabled")
    endif()
    
    if(HAVE_SYS_SIGNALFD_H)
        target_compile_definitions(code_server PRIVATE HAVE_SIGNALFD=1)
        message(STATUS "Server: signalfd support enabled")
    endif()
    
elseif(APPLE)
    # macOS specific features
    target_compile_definitions(code_server PRIVATE MACOS_PLATFORM=1)
    
    # Check for kqueue
    include(CheckIncludeFile)
    check_include_file(sys/event.h HAVE_SYS_EVENT_H)
    if(HAVE_SYS_EVENT_H)
        target_compile_definitions(code_server PRIVATE HAVE_KQUEUE=1)
        message(STATUS "Server: kqueue support enabled")
    endif()
    
elseif(WIN32)
    # Windows specific features
    target_compile_definitions(code_server PRIVATE WINDOWS_PLATFORM=1)
    target_link_libraries(code_server PRIVATE ws2_32 mswsock)
endif()

# Verificarea pentru biblioteci opționale
find_library(LIBM_LIBRARY m)
if(LIBM_LIBRARY)
    target_link_libraries(code_server PRIVATE ${LIBM_LIBRARY})
endif()

# Support pentru JSON parsing (pentru configurare avansată)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(JSON_C json-c)
    if(JSON_C_FOUND)
        target_link_libraries(code_server PRIVATE ${JSON_C_LIBRARIES})
        target_include_directories(code_server PRIVATE ${JSON_C_INCLUDE_DIRS})
        target_compile_definitions(code_server PRIVATE HAVE_JSON_C=1)
        message(STATUS "Server: JSON-C support enabled")
    endif()
endif()

# Configurarea pentru debugging
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(code_server PRIVATE
        DEBUG_LOGGING=1
        VERBOSE_LOGGING=1
    )
endif()

# Proprietăți specifice executabilului
set_target_properties(code_server PROPERTIES
    OUTPUT_NAME "code_server"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Custom target pentru crearea directoarelor necesare în timpul build-ului
add_custom_target(create_server_dirs ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/logs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/processing
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/outgoing
    COMMENT "Creating server runtime directories"
)

add_dependencies(code_server create_server_dirs)

# Configurarea instalării
install(TARGETS code_server
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

# Instalarea fișierelor de configurare
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/config/server.conf.example
    DESTINATION etc/code_server
    COMPONENT Configuration
    OPTIONAL
)

# Script de inițializare pentru systemd (Linux)
if(UNIX AND NOT APPLE)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/code_server.service.in
        ${CMAKE_CURRENT_BINARY_DIR}/code_server.service
        @ONLY
    )
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/code_server.service
        DESTINATION lib/systemd/system
        COMPONENT SystemIntegration
        OPTIONAL
    )
endif()

# Target pentru rularea serverului în modul development
add_custom_target(run_server
    COMMAND ${CMAKE_BINARY_DIR}/bin/code_server --config ${CMAKE_SOURCE_DIR}/server/config/dev.conf
    DEPENDS code_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running server in development mode"
)

# Verificarea și warning-uri pentru runtime dependencies
find_program(GCC_EXECUTABLE gcc)
find_program(GPP_EXECUTABLE g++)
find_program(PYTHON_EXECUTABLE python3)
find_program(JAVA_EXECUTABLE java)
find_program(JAVAC_EXECUTABLE javac)
find_program(NODE_EXECUTABLE node)

if(NOT GCC_EXECUTABLE)
    message(WARNING "gcc not found - C compilation will not be available")
endif()

if(NOT GPP_EXECUTABLE)
    message(WARNING "g++ not found - C++ compilation will not be available")
endif()

if(NOT PYTHON_EXECUTABLE)
    message(WARNING "python3 not found - Python execution will not be available")
endif()

if(NOT JAVA_EXECUTABLE OR NOT JAVAC_EXECUTABLE)
    message(WARNING "Java tools not found - Java compilation will not be available")
endif()

if(NOT NODE_EXECUTABLE)
    message(WARNING "Node.js not found - JavaScript execution will not be available")
endif()

message(STATUS "Server build configuration completed")