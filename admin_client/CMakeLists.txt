# admin_client/CMakeLists.txt
# Client de administrare pentru serverul de compilare

# Lista fișierelor sursă
set(ADMIN_CLIENT_SOURCES
    src/admin_client.cpp
    src/admin_commands.cpp
)

# Lista header-urilor
set(ADMIN_CLIENT_HEADERS
    include/admin_client.h
    include/admin_commands.h
)

# Crearea executabilului admin client
add_executable(admin_client ${ADMIN_CLIENT_SOURCES} ${ADMIN_CLIENT_HEADERS})

# Alias pentru claritate
add_executable(CodeCompiler::admin_client ALIAS admin_client)

# Configurarea directoarelor include
target_include_directories(admin_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Linkarea cu biblioteca comună
target_link_libraries(admin_client PRIVATE
    CodeCompiler::common
)

# Definirea macro-urilor specifice admin client-ului
target_compile_definitions(admin_client PRIVATE
    ADMIN_CLIENT_VERSION="${PROJECT_VERSION}"
    DEFAULT_SOCKET_PATH="/tmp/code_server_admin.sock"
    DEFAULT_CONFIG_FILE="/etc/code_server/admin.conf"
)

# Support pentru ncurses (interfață avansată în terminal)
if(BUILD_WITH_NCURSES)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NCURSES ncurses)
        if(NCURSES_FOUND)
            target_link_libraries(admin_client PRIVATE ${NCURSES_LIBRARIES})
            target_include_directories(admin_client PRIVATE ${NCURSES_INCLUDE_DIRS})
            target_compile_options(admin_client PRIVATE ${NCURSES_CFLAGS_OTHER})
            target_compile_definitions(admin_client PRIVATE 
                HAVE_NCURSES=1
                NCURSES_INTERFACE=1
            )
            message(STATUS "Admin Client: ncurses support enabled")
        else()
            message(STATUS "Admin Client: ncurses not found, using simple text interface")
        endif()
    endif()
endif()

# Support pentru readline (pentru command history și auto-completion)
find_library(READLINE_LIBRARY readline)
find_path(READLINE_INCLUDE_DIR readline/readline.h)

if(READLINE_LIBRARY AND READLINE_INCLUDE_DIR)
    target_link_libraries(admin_client PRIVATE ${READLINE_LIBRARY})
    target_include_directories(admin_client PRIVATE ${READLINE_INCLUDE_DIR})
    target_compile_definitions(admin_client PRIVATE HAVE_READLINE=1)
    message(STATUS "Admin Client: readline support enabled")
    
    # Readline may require termcap or ncurses
    find_library(TERMCAP_LIBRARY termcap)
    if(TERMCAP_LIBRARY)
        target_link_libraries(admin_client PRIVATE ${TERMCAP_LIBRARY})
    endif()
endif()

# Platform-specific configurations
if(UNIX)
    target_compile_definitions(admin_client PRIVATE UNIX_PLATFORM=1)
    
    # Check for advanced UNIX features
    include(CheckIncludeFile)
    check_include_file(sys/un.h HAVE_SYS_UN_H)
    if(HAVE_SYS_UN_H)
        target_compile_definitions(admin_client PRIVATE HAVE_UNIX_SOCKETS=1)
    endif()
    
elseif(WIN32)
    target_compile_definitions(admin_client PRIVATE WINDOWS_PLATFORM=1)
    # Windows folosește named pipes în loc de UNIX sockets
    target_compile_definitions(admin_client PRIVATE USE_NAMED_PIPES=1)
endif()

# Support pentru JSON parsing (pentru output formatat)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(JSON_C json-c)
    if(JSON_C_FOUND)
        target_link_libraries(admin_client PRIVATE ${JSON_C_LIBRARIES})
        target_include_directories(admin_client PRIVATE ${JSON_C_INCLUDE_DIRS})
        target_compile_definitions(admin_client PRIVATE HAVE_JSON_C=1)
        message(STATUS "Admin Client: JSON-C support enabled for formatted output")
    endif()
endif()

# Configurarea pentru debugging
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(admin_client PRIVATE
        DEBUG_LOGGING=1
        VERBOSE_ADMIN_LOGGING=1
    )
endif()

# Proprietăți specifice executabilului
set_target_properties(admin_client PROPERTIES
    OUTPUT_NAME "admin_client"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Configurarea instalării
install(TARGETS admin_client
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

# Instalarea fișierelor de configurare
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/config/admin.conf.example
    DESTINATION etc/code_server
    COMPONENT Configuration
    OPTIONAL
)

# Instalarea fișierelor de help și documentație
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/docs/admin_commands.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/docs/admin_usage.md
    DESTINATION share/doc/code_server
    COMPONENT Documentation
    OPTIONAL
)

# Target pentru rularea admin client-ului în modul development
add_custom_target(run_admin
    COMMAND ${CMAKE_BINARY_DIR}/bin/admin_client --socket ${CMAKE_BINARY_DIR}/server_admin.sock
    DEPENDS admin_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running admin client in development mode"
)

# Target pentru generarea bash completion script-ului
find_program(BASH_EXECUTABLE bash)
if(BASH_EXECUTABLE)
    add_custom_target(generate_completion
        COMMAND ${CMAKE_BINARY_DIR}/bin/admin_client --generate-completion > ${CMAKE_BINARY_DIR}/admin_client_completion.bash
        DEPENDS admin_client
        COMMENT "Generating bash completion script"
    )
    
    # Instalarea completion script-ului
    install(FILES ${CMAKE_BINARY_DIR}/admin_client_completion.bash
        DESTINATION share/bash-completion/completions
        RENAME admin_client
        COMPONENT BashCompletion
        OPTIONAL
    )
endif()

# Verificarea dependințelor runtime
if(UNIX)
    find_program(SOCAT_EXECUTABLE socat)
    if(NOT SOCAT_EXECUTABLE)
        message(STATUS "socat not found - some admin features may be limited")
    endif()
endif()

# Configurare pentru integrare cu IDE-uri
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    set_target_properties(admin_client PROPERTIES
        COMPILE_FLAGS "-DIDE_INTEGRATION=1"
    )
endif()

message(STATUS "Admin Client build configuration completed")