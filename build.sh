#!/bin/bash
set -e

BUILD_DIR="build-windows"
JSON_URL="https://github.com/nlohmann/json/releases/latest/download/json.hpp"
TOOLCHAIN="toolchain-mingw64.cmake"

echo "=== CatUpdate Windows Cross-Compile Script ==="

# 1. Check for dependencies
if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "Error: MinGW-w64 cross-compiler (x86_64-w64-mingw32-g++) not found."
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "Error: cmake not found."
    exit 1
fi

# 2. Download json.hpp if missing
if [ ! -f "include/json.hpp" ]; then
    echo "Downloading nlohmann/json..."
    mkdir -p include
    curl -L "$JSON_URL" -o include/json.hpp
fi

# 3. Setup build directory
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

# 4. Configure and Build
echo "Configuring with CMake..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release

echo "Building targets..."
cmake --build "$BUILD_DIR" --config Release

echo "=== Build Complete ==="
echo "CLI Binary:   $BUILD_DIR/catupdate.exe"
echo "GUI Binary:   $BUILD_DIR/catupdate-gui.exe"
echo "Tests Binary: $BUILD_DIR/catupdate-tests.exe"
