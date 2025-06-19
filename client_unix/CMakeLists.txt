# client_unix/CMakeLists.txt
# Client ordinar pentru sistemele UNIX/Linux

# Lista fișierelor sursă
set(CLIENT_SOURCES
    src/client.cpp
    src/file_transfer.cpp
    src/communication.cpp
)

# Lista header-urilor
set(CLIENT_HEADERS
    include/client.h
    include/communication.h
)

# Crearea executabilului client
add_executable(code_client ${CLIENT_SOURCES} ${CLIENT_HEADERS})

# Alias pentru claritate
add_executable(CodeCompiler::client ALIAS code_client)

# Configurarea directoarelor include
target_include_directories(code_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Linkarea cu biblioteca comună
target_link_libraries(code_client PRIVATE
    CodeCompiler::common
)

# Definirea macro-urilor specifice client-ului
target_compile_definitions(code_client PRIVATE
    CLIENT_VERSION="${PROJECT_VERSION}"
    DEFAULT_SERVER_HOST="localhost"
    DEFAULT_SERVER_PORT=8080
    DEFAULT_CONFIG_FILE="~/.config/code_client/client.conf"
    DEFAULT_CACHE_DIR="~/.cache/code_client"
)

# Platform-specific configurations
if(UNIX AND NOT APPLE)
    target_compile_definitions(code_client PRIVATE LINUX_PLATFORM=1)
    
    # Check for Linux-specific features
    include(CheckIncludeFile)
    check_include_file(sys/sendfile.h HAVE_SYS_SENDFILE_H)
    if(HAVE_SYS_SENDFILE_H)
        target_compile_definitions(code_client PRIVATE HAVE_SENDFILE=1)
        message(STATUS "Client: sendfile support enabled for efficient file transfers")
    endif()
    
elseif(APPLE)
    target_compile_definitions(code_client PRIVATE MACOS_PLATFORM=1)
    
elseif(WIN32)
    target_compile_definitions(code_client PRIVATE WINDOWS_PLATFORM=1)
    target_link_libraries(code_client PRIVATE ws2_32)
endif()

# Support pentru SSL/TLS (opțional, pentru comunicare securizată)
find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
    target_link_libraries(code_client PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(code_client PRIVATE 
        HAVE_OPENSSL=1
        SSL_SUPPORT=1
    )
    message(STATUS "Client: OpenSSL support enabled")
endif()

# Support pentru compresie (pentru transfer eficient de fișiere)
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_link_libraries(code_client PRIVATE ZLIB::ZLIB)
    target_compile_definitions(code_client PRIVATE HAVE_ZLIB=1)
    message(STATUS "Client: ZLIB compression support enabled")
endif()

# Support pentru progress bar în terminal
find_library(NCURSES_LIBRARY ncurses)
if(NCURSES_LIBRARY)
    target_link_libraries(code_client PRIVATE ${NCURSES_LIBRARY})
    target_compile_definitions(code_client PRIVATE HAVE_NCURSES_PROGRESS=1)
    message(STATUS "Client: ncurses progress bar support enabled")
endif()

# Support pentru configurare avansată
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(JSON_C json-c)
    if(JSON_C_FOUND)
        target_link_libraries(code_client PRIVATE ${JSON_C_LIBRARIES})
        target_include_directories(code_client PRIVATE ${JSON_C_INCLUDE_DIRS})
        target_compile_definitions(code_client PRIVATE HAVE_JSON_C=1)
        message(STATUS "Client: JSON-C support enabled for configuration")
    endif()
endif()

# Support pentru async I/O
include(CheckIncludeFile)
check_include_file(aio.h HAVE_AIO_H)
if(HAVE_AIO_H)
    target_compile_definitions(code_client PRIVATE HAVE_AIO=1)
    target_link_libraries(code_client PRIVATE rt)
    message(STATUS "Client: Async I/O support enabled")
endif()

# Support pentru file watching (pentru auto-sync)
if(UNIX AND NOT APPLE)
    check_include_file(sys/inotify.h HAVE_SYS_INOTIFY_H)
    if(HAVE_SYS_INOTIFY_H)
        target_compile_definitions(code_client PRIVATE HAVE_INOTIFY=1)
        message(STATUS "Client: inotify file watching support enabled")
    endif()
elseif(APPLE)
    check_include_file(sys/event.h HAVE_SYS_EVENT_H)
    if(HAVE_SYS_EVENT_H)
        target_compile_definitions(code_client PRIVATE HAVE_KQUEUE=1)
        message(STATUS "Client: kqueue file watching support enabled")
    endif()
endif()

# Support pentru UUID generation (pentru correlation IDs)
find_library(UUID_LIBRARY uuid)
if(UUID_LIBRARY)
    target_link_libraries(code_client PRIVATE ${UUID_LIBRARY})
    target_compile_definitions(code_client PRIVATE HAVE_UUID=1)
    message(STATUS "Client: UUID support enabled")
endif()

# Support pentru cURL (pentru HTTP-based API calls)
find_package(CURL QUIET)
if(CURL_FOUND)
    target_link_libraries(code_client PRIVATE CURL::libcurl)
    target_compile_definitions(code_client PRIVATE HAVE_CURL=1)
    message(STATUS "Client: cURL support enabled for HTTP API")
endif()

# Support pentru readline pentru interfață interactivă
find_library(READLINE_LIBRARY readline)
find_path(READLINE_INCLUDE_DIR readline/readline.h)

if(READLINE_LIBRARY AND READLINE_INCLUDE_DIR)
    target_link_libraries(code_client PRIVATE ${READLINE_LIBRARY})
    target_include_directories(code_client PRIVATE ${READLINE_INCLUDE_DIR})
    target_compile_definitions(code_client PRIVATE HAVE_READLINE=1)
    message(STATUS "Client: readline support enabled for interactive mode")
    
    # Readline may require termcap
    find_library(TERMCAP_LIBRARY termcap)
    if(TERMCAP_LIBRARY)
        target_link_libraries(code_client PRIVATE ${TERMCAP_LIBRARY})
    endif()
endif()

# Support pentru syntax highlighting în output
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBXML2 libxml-2.0)
    if(LIBXML2_FOUND)
        target_link_libraries(code_client PRIVATE ${LIBXML2_LIBRARIES})
        target_include_directories(code_client PRIVATE ${LIBXML2_INCLUDE_DIRS})
        target_compile_definitions(code_client PRIVATE HAVE_LIBXML2=1)
        message(STATUS "Client: libxml2 support enabled for XML parsing")
    endif()
endif()

# Support pentru terminal color output
check_include_file(termios.h HAVE_TERMIOS_H)
if(HAVE_TERMIOS_H)
    target_compile_definitions(code_client PRIVATE HAVE_TERMIOS=1)
    message(STATUS "Client: Terminal color support enabled")
endif()

# Configurarea pentru debugging
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(code_client PRIVATE
        DEBUG_LOGGING=1
        VERBOSE_CLIENT_LOGGING=1
        NETWORK_DEBUG=1
        FILE_TRANSFER_DEBUG=1
    )
endif()

# Proprietăți specifice executabilului
set_target_properties(code_client PROPERTIES
    OUTPUT_NAME "code_client"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Configurarea instalării
install(TARGETS code_client
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

# Instalarea fișierelor de configurare
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/config/client.conf.example
    DESTINATION etc/code_server
    COMPONENT Configuration
    OPTIONAL
)

# Instalarea template-urilor de cod
install(DIRECTORY 
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/
    DESTINATION share/code_client/templates
    COMPONENT Templates
    OPTIONAL
    PATTERN "*.template"
)

# Instalarea documentației
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/docs/client_usage.md
    ${CMAKE_CURRENT_SOURCE_DIR}/docs/api_examples.md
    ${CMAKE_CURRENT_SOURCE_DIR}/docs/troubleshooting.md
    DESTINATION share/doc/code_server
    COMPONENT Documentation
    OPTIONAL
)

# Instalarea manualelor
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/man/code_client.1
    DESTINATION share/man/man1
    COMPONENT Documentation
    OPTIONAL
)

# Target pentru rularea client-ului în modul development
add_custom_target(run_client
    COMMAND ${CMAKE_BINARY_DIR}/bin/code_client --server localhost:8080 --config ${CMAKE_SOURCE_DIR}/client_unix/config/dev.conf
    DEPENDS code_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running client in development mode"
)

# Target pentru demo interactiv
add_custom_target(demo_client
    COMMAND ${CMAKE_BINARY_DIR}/bin/code_client --demo --server localhost:8080
    DEPENDS code_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running client demo"
)

# Target pentru interfața interactivă
add_custom_target(interactive_client
    COMMAND ${CMAKE_BINARY_DIR}/bin/code_client --interactive --server localhost:8080
    DEPENDS code_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running client in interactive mode"
)

# Crearea unui script wrapper pentru ușurința utilizării
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/code_client_wrapper.sh.in
    ${CMAKE_BINARY_DIR}/bin/code_client_wrapper.sh
    @ONLY
)

# Setarea permisiunilor pentru script
if(UNIX)
    file(COPY ${CMAKE_BINARY_DIR}/bin/code_client_wrapper.sh
         DESTINATION ${CMAKE_BINARY_DIR}/bin
         FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                         GROUP_READ GROUP_EXECUTE
                         WORLD_READ WORLD_EXECUTE)
endif()

# Instalarea script-ului wrapper
install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/code_client_wrapper.sh
    DESTINATION bin
    RENAME code_submit
    COMPONENT Runtime
)

# Crearea script-ului de quick compile
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/quick_compile.sh.in
    ${CMAKE_BINARY_DIR}/bin/quick_compile.sh
    @ONLY
)

if(UNIX)
    file(COPY ${CMAKE_BINARY_DIR}/bin/quick_compile.sh
         DESTINATION ${CMAKE_BINARY_DIR}/bin
         FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                         GROUP_READ GROUP_EXECUTE
                         WORLD_READ WORLD_EXECUTE)
endif()

install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/quick_compile.sh
    DESTINATION bin
    COMPONENT Runtime
)

# Target pentru testarea conexiunii
add_custom_target(test_connection
    COMMAND ${CMAKE_BINARY_DIR}/bin/code_client --test-connection --server localhost:8080
    DEPENDS code_client
    COMMENT "Testing connection to server"
)

# Target pentru benchmark
add_custom_target(benchmark_client
    COMMAND ${CMAKE_BINARY_DIR}/bin/code_client --benchmark --server localhost:8080
    DEPENDS code_client
    COMMENT "Running client benchmark"
)

# Verificarea dependințelor runtime pentru diverse limbaje
find_program(GCC_EXECUTABLE gcc)
find_program(GPP_EXECUTABLE g++)
find_program(PYTHON_EXECUTABLE python3)
find_program(JAVA_EXECUTABLE java)
find_program(JAVAC_EXECUTABLE javac)
find_program(NODE_EXECUTABLE node)
find_program(GO_EXECUTABLE go)
find_program(RUSTC_EXECUTABLE rustc)

# Configurarea path-urilor pentru compilatoare
if(GCC_EXECUTABLE)
    target_compile_definitions(code_client PRIVATE GCC_PATH="${GCC_EXECUTABLE}")
    message(STATUS "Client: Found gcc at ${GCC_EXECUTABLE}")
endif()

if(GPP_EXECUTABLE)
    target_compile_definitions(code_client PRIVATE GPP_PATH="${GPP_EXECUTABLE}")
    message(STATUS "Client: Found g++ at ${GPP_EXECUTABLE}")
endif()

if(PYTHON_EXECUTABLE)
    target_compile_definitions(code_client PRIVATE PYTHON_PATH="${PYTHON_EXECUTABLE}")
    message(STATUS "Client: Found python3 at ${PYTHON_EXECUTABLE}")
endif()

if(JAVA_EXECUTABLE AND JAVAC_EXECUTABLE)
    target_compile_definitions(code_client PRIVATE 
        JAVA_PATH="${JAVA_EXECUTABLE}"
        JAVAC_PATH="${JAVAC_EXECUTABLE}")
    message(STATUS "Client: Found Java tools")
endif()

if(NODE_EXECUTABLE)
    target_compile_definitions(code_client PRIVATE NODE_PATH="${NODE_EXECUTABLE}")
    message(STATUS "Client: Found Node.js at ${NODE_EXECUTABLE}")
endif()

if(GO_EXECUTABLE)
    target_compile_definitions(code_client PRIVATE GO_PATH="${GO_EXECUTABLE}")
    message(STATUS "Client: Found Go at ${GO_EXECUTABLE}")
endif()

if(RUSTC_EXECUTABLE)
    target_compile_definitions(code_client PRIVATE RUSTC_PATH="${RUSTC_EXECUTABLE}")
    message(STATUS "Client: Found Rust at ${RUSTC_EXECUTABLE}")
endif()

# Target pentru instalarea completions pentru bash/zsh
find_program(BASH_EXECUTABLE bash)
if(BASH_EXECUTABLE)
    add_custom_target(generate_bash_completion
        COMMAND ${CMAKE_BINARY_DIR}/bin/code_client --generate-bash-completion > ${CMAKE_BINARY_DIR}/code_client_completion.bash
        DEPENDS code_client
        COMMENT "Generating bash completion"
    )
    
    install(FILES ${CMAKE_BINARY_DIR}/code_client_completion.bash
        DESTINATION share/bash-completion/completions
        RENAME code_client
        COMPONENT BashCompletion
        OPTIONAL
    )
endif()

# Target pentru instalarea zsh completion
find_program(ZSH_EXECUTABLE zsh)
if(ZSH_EXECUTABLE)
    add_custom_target(generate_zsh_completion
        COMMAND ${CMAKE_BINARY_DIR}/bin/code_client --generate-zsh-completion > ${CMAKE_BINARY_DIR}/_code_client
        DEPENDS code_client
        COMMENT "Generating zsh completion"
    )
    
    install(FILES ${CMAKE_BINARY_DIR}/_code_client
        DESTINATION share/zsh/site-functions
        COMPONENT ZshCompletion
        OPTIONAL
    )
endif()

# Crearea desktop entry pentru GUI integration
if(UNIX AND NOT APPLE)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/desktop/code_client.desktop.in
        ${CMAKE_BINARY_DIR}/code_client.desktop
        @ONLY
    )
    
    install(FILES ${CMAKE_BINARY_DIR}/code_client.desktop
        DESTINATION share/applications
        COMPONENT DesktopIntegration
        OPTIONAL
    )
endif()

# Post-install script pentru actualizarea cache-urilor sistem
if(UNIX AND NOT APPLE)
    install(CODE "
        execute_process(COMMAND ldconfig)
        execute_process(COMMAND update-desktop-database)
        execute_process(COMMAND update-mime-database /usr/share/mime)
    " COMPONENT Runtime OPTIONAL)
endif()

# Verificarea întregirății build-ului
add_custom_target(verify_client_build
    COMMAND ${CMAKE_BINARY_DIR}/bin/code_client --version
    COMMAND ${CMAKE_BINARY_DIR}/bin/code_client --help
    DEPENDS code_client
    COMMENT "Verifying client build integrity"
)

# Performance profiling target (doar pentru debug)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_program(PERF_EXECUTABLE perf)
    if(PERF_EXECUTABLE)
        add_custom_target(profile_client
            COMMAND ${PERF_EXECUTABLE} record -g ${CMAKE_BINARY_DIR}/bin/code_client --benchmark --server localhost:8080
            DEPENDS code_client
            COMMENT "Profiling client performance"
        )
    endif()
endif()

# Memory profiling cu Valgrind
find_program(VALGRIND_EXECUTABLE valgrind)
if(VALGRIND_EXECUTABLE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_target(memcheck_client
        COMMAND ${VALGRIND_EXECUTABLE} --tool=memcheck --leak-check=full --show-leak-kinds=all
                ${CMAKE_BINARY_DIR}/bin/code_client --test-connection --server localhost:8080
        DEPENDS code_client
        COMMENT "Running memory check on client"
    )
endif()

# Packaging pentru distribution
set(CPACK_PACKAGE_NAME "code-client")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Code Compiler & Executer Client")
set(CPACK_PACKAGE_VENDOR "PCD Project Team")

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgcc1, libstdc++6")
    set(CPACK_RPM_PACKAGE_REQUIRES "glibc, gcc, libstdc++")
endif()

message(STATUS "Unix Client build configuration completed")
message(STATUS "Available client targets:")
message(STATUS "  - run_client: Run in development mode")
message(STATUS "  - demo_client: Run demo")
message(STATUS "  - interactive_client: Interactive mode")
message(STATUS "  - test_connection: Test server connection")
message(STATUS "  - benchmark_client: Performance benchmark")
message(STATUS "  - verify_client_build: Verify build integrity")