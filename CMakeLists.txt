cmake_minimum_required(VERSION 3.20)
project(CatUpdate VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands database for language servers and static analysis
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find and configure clang-tidy static analysis tool
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(NOT CLANG_TIDY_EXE)
    execute_process(
        COMMAND brew --prefix llvm
        OUTPUT_VARIABLE BREW_LLVM_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(BREW_LLVM_PREFIX)
        find_program(CLANG_TIDY_EXE NAMES clang-tidy HINTS "${BREW_LLVM_PREFIX}/bin")
    endif()
endif()

if(CLANG_TIDY_EXE AND NOT CMAKE_CROSSCOMPILING)
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})
endif()

# =============================================================================
# Core Static Library Target
# =============================================================================
add_library(catupdate-core STATIC
    src/CatUpdateCore.cpp
    src/PackageManager.cpp
    src/PlatformTraits.cpp
    src/Providers.cpp
)

# Header search paths for the library and its dependents
target_include_directories(catupdate-core PUBLIC include)
target_include_directories(catupdate-core SYSTEM PRIVATE third_party)

# Platform-specific abstraction sources
if(WIN32)
    target_sources(catupdate-core PRIVATE
        src/win32/CatUpdateCoreWin32.cpp
        src/win32/HttpClientWin32.cpp
        src/win32/ProcessExecutorWin32.cpp
        src/win32/PlatformTraitsWin32.cpp
    )
else()
    target_sources(catupdate-core PRIVATE
        src/posix/CatUpdateCorePosix.cpp
        src/posix/HttpClientPosix.cpp
        src/posix/ProcessExecutorPosix.cpp
        src/posix/PlatformTraitsPosix.cpp
    )
endif()

# =============================================================================
# Platform-Specific Compilation Flags & Linkage
# =============================================================================
if(WIN32)
    # Link native Windows SDK libraries
    target_link_libraries(catupdate-core PUBLIC
        winhttp
        shlwapi
        ole32
    )

    # Unicode Support and modern Windows API Targets (Windows 10+)
    target_compile_definitions(catupdate-core PUBLIC UNICODE _UNICODE _WIN32_WINNT=0x0A00)
    target_compile_options(catupdate-core PUBLIC -municode)
    target_link_options(catupdate-core PUBLIC -municode -static)

    # Stripping binary symbols in Release configuration (MinGW optimization)
    target_link_options(catupdate-core PUBLIC $<$<CONFIG:Release>:-s>)
else()
    # POSIX threads linkage for standard threads on Linux/macOS
    find_package(Threads REQUIRED)
    target_link_libraries(catupdate-core PUBLIC Threads::Threads)
endif()

# =============================================================================
# Compilation Target: Command-Line Interface (CLI)
# =============================================================================
add_executable(catupdate src/cli_main.cpp)
target_link_libraries(catupdate PRIVATE catupdate-core)

# =============================================================================
# Compilation Target: Unit Test Suite
# =============================================================================
add_executable(catupdate-tests
    tests/test_main.cpp
    tests/test_core.cpp
    tests/test_providers.cpp
)
target_link_libraries(catupdate-tests PRIVATE catupdate-core)

# =============================================================================
# Compilation Target: Win32 Demoscene GUI (Windows-only)
# =============================================================================
if(WIN32)
    add_executable(catupdate-gui WIN32
        src/gui_main.cpp
        resources/resources.rc
    )
    
    target_link_libraries(catupdate-gui PRIVATE
        catupdate-core
        comctl32
        gdi32
        user32
        kernel32
    )
endif()
