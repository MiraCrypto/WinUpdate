cmake_minimum_required(VERSION 3.20)
project(CatUpdate VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# -----------------------------------------------------------------------------
# Common Platform-Independent Core Sources
# -----------------------------------------------------------------------------
set(CORE_SOURCES
    src/CatUpdateCore.cpp
    src/Providers.cpp
)

# -----------------------------------------------------------------------------
# Platform-Specific Compilation Units
# -----------------------------------------------------------------------------
if(WIN32)
    set(PLATFORM_SOURCES
        src/win32/CatUpdateCoreWin32.cpp
        src/win32/HttpClientWin32.cpp
        src/win32/ProcessExecutorWin32.cpp
        src/win32/PlatformTraitsWin32.cpp
    )
else()
    set(PLATFORM_SOURCES
        src/posix/CatUpdateCorePosix.cpp
        src/posix/HttpClientPosix.cpp
        src/posix/ProcessExecutorPosix.cpp
        src/posix/PlatformTraitsPosix.cpp
    )
endif()

# -----------------------------------------------------------------------------
# Compilation Target: Command-Line Interface (CLI)
# -----------------------------------------------------------------------------
add_executable(catupdate
    src/cli_main.cpp
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
)

# -----------------------------------------------------------------------------
# Compilation Target: Unit Test Suite
# -----------------------------------------------------------------------------
add_executable(catupdate-tests
    tests/test_main.cpp
    tests/test_core.cpp
    tests/test_providers.cpp
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
)

# -----------------------------------------------------------------------------
# Platform Flags & Libraries Linking
# -----------------------------------------------------------------------------
if(WIN32)
    # Link native Windows libraries
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

    # Compilation Target: Win32 Demoscene GUI (Windows-only)
    set(GUI_SOURCES
        src/gui_main.cpp
        resources/resources.rc
        ${CORE_SOURCES}
        ${PLATFORM_SOURCES}
    )

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

    # Unicode Support and Modern Windows API Targets (Windows 10+)
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
    # POSIX threads linkage for standard threads on Linux
    find_package(Threads REQUIRED)
    target_link_libraries(catupdate PRIVATE Threads::Threads)
    target_link_libraries(catupdate-tests PRIVATE Threads::Threads)
endif()

# Stripping binary symbols in Release mode (Windows MinGW optimizations)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(WIN32)
        set_target_properties(catupdate PROPERTIES LINK_FLAGS "-s")
        set_target_properties(catupdate-gui PROPERTIES LINK_FLAGS "-s")
        set_target_properties(catupdate-tests PROPERTIES LINK_FLAGS "-s")
    endif()
endif()
