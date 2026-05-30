cmake_minimum_required(VERSION 3.20)
project(WinUpdate VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Link necessary Windows libraries
link_libraries(
    winhttp
    comctl32
    ole32
    shlwapi
)

# Source files
set(SOURCES
    src/main.cpp
    src/WinUpdateCore.cpp
    resources/resources.rc
)

# WIN32 property adds -mwindows for MinGW
add_executable(WinUpdate WIN32 ${SOURCES})

# Unicode support and static linking for MinGW
target_compile_definitions(WinUpdate PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0601)
target_compile_options(WinUpdate PRIVATE -municode)
target_link_options(WinUpdate PRIVATE -municode -static)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(WinUpdate PROPERTIES LINK_FLAGS "-s")
endif()
