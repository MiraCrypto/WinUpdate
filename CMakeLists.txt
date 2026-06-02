cmake_minimum_required(VERSION 3.20)
project(CatUpdate VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# -----------------------------------------------------------------------------
# Cross-Platform Sources and Targets
# -----------------------------------------------------------------------------

# Common library source files
set(CORE_SOURCES
    src/CatUpdateCore.cpp
    src/HttpClient.cpp
    src/ProcessExecutor.cpp
    src/Providers.cpp
)

# Add CLI executable (Runs on Windows, Linux, macOS)
add_executable(catupdate
    src/cli_main.cpp
    ${CORE_SOURCES}
)

# Add Unit Tests executable
add_executable(catupdate-tests
    tests/test_main.cpp
    tests/test_core.cpp
    tests/test_providers.cpp
    ${CORE_SOURCES}
)

# -----------------------------------------------------------------------------
# Platform Specific Flags & Libraries
# -----------------------------------------------------------------------------

if(WIN32)
    # Link Windows libraries
    target_link_libraries(catupdate PRIVATE
        winhttp
        shlwapi
        ole32
    )
    target_link_libraries(catupdate-tests PRIVATE
        winhttp
        shlwapi
        ole32
    )

    # Define GUI executable source
    set(GUI_SOURCES
        src/gui_main.cpp
        resources/resources.rc
        ${CORE_SOURCES}
    )

    # GUI target (WIN32 adds -mwindows for MinGW cross compile)
    add_executable(catupdate-gui WIN32 ${GUI_SOURCES})
    
    target_link_libraries(catupdate-gui PRIVATE
        winhttp
        comctl32
        ole32
        shlwapi
        gdi32
        user32
        kernel32
    )

    # Enable Unicode, target modern Windows 10+
    target_compile_definitions(catupdate PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)
    target_compile_definitions(catupdate-tests PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)
    target_compile_definitions(catupdate-gui PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)

    target_compile_options(catupdate PRIVATE -municode)
    target_compile_options(catupdate-tests PRIVATE -municode)
    target_compile_options(catupdate-gui PRIVATE -municode)

    target_link_options(catupdate PRIVATE -municode -static)
    target_link_options(catupdate-tests PRIVATE -municode -static)
    target_link_options(catupdate-gui PRIVATE -municode -static)
else()
    # Linux/macOS - standard configurations
    # Under Linux, standard pthread might be needed for std::thread
    find_package(Threads REQUIRED)
    target_link_libraries(catupdate PRIVATE Threads::Threads)
    target_link_libraries(catupdate-tests PRIVATE Threads::Threads)
endif()

# Stripping binary in Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(WIN32)
        set_target_properties(catupdate PROPERTIES LINK_FLAGS "-s")
        set_target_properties(catupdate-gui PROPERTIES LINK_FLAGS "-s")
        set_target_properties(catupdate-tests PROPERTIES LINK_FLAGS "-s")
    endif()
endif()
