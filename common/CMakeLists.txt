# common/CMakeLists.txt
# Biblioteca comună pentru protocoale și structuri partajate

# Crearea unei biblioteci interface pentru header-urile comune
add_library(common INTERFACE)

# Definirea alias-ului pentru claritate
add_library(CodeCompiler::common ALIAS common)

# Expunerea directoarelor include
target_include_directories(common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Lista header-urilor comune
set(COMMON_HEADERS
    protocol.h
    messages.h
    utils.h
)

# Adăugarea header-urilor ca surse pentru IDE-uri
target_sources(common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/protocol.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/messages.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utils.h>
)

# Definirea unor macro-uri comune
target_compile_definitions(common INTERFACE
    $<$<CONFIG:Debug>:DEBUG_MODE=1>
    $<$<CONFIG:Release>:RELEASE_MODE=1>
    PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Cerințe pentru threading (toate componentele folosesc thread-uri)
target_link_libraries(common INTERFACE Threads::Threads)

# Platform-specific dependencies
if(UNIX)
    target_link_libraries(common INTERFACE rt)
endif()

if(WIN32)
    target_link_libraries(common INTERFACE ws2_32)
    target_compile_definitions(common INTERFACE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
    )
endif()

# Instalarea header-urilor
install(FILES ${COMMON_HEADERS}
    DESTINATION include/code_compiler
    COMPONENT Development
)

# Instalarea target-ului pentru find_package
install(TARGETS common
    EXPORT CodeCompilerTargets
    INCLUDES DESTINATION include
)

# Crearea fișierului de configurare pentru find_package
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CodeCompilerConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/CodeCompilerConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CodeCompilerConfig.cmake
    INSTALL_DESTINATION lib/cmake/CodeCompiler
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CodeCompilerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CodeCompilerConfigVersion.cmake
    DESTINATION lib/cmake/CodeCompiler
    COMPONENT Development
)

install(EXPORT CodeCompilerTargets
    FILE CodeCompilerTargets.cmake
    NAMESPACE CodeCompiler::
    DESTINATION lib/cmake/CodeCompiler
    COMPONENT Development
)