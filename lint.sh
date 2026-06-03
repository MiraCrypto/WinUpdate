#!/bin/bash
# =============================================================================
# CatUpdate Linting & Formatting Workflow Script
# =============================================================================
set -e

# Dynamically prepend Homebrew LLVM path if available to ensure correct tool versions
if command -v brew &> /dev/null; then
    BREW_LLVM_PREFIX=$(brew --prefix llvm 2>/dev/null || echo "")
    if [ -n "$BREW_LLVM_PREFIX" ] && [ -d "$BREW_LLVM_PREFIX/bin" ]; then
        export PATH="$BREW_LLVM_PREFIX/bin:$PATH"
    fi
fi

CLANG_FORMAT="clang-format"
CLANG_TIDY="clang-tidy"

if ! command -v "$CLANG_FORMAT" &> /dev/null; then
    echo "Error: $CLANG_FORMAT not found in PATH."
    exit 1
fi

if ! command -v "$CLANG_TIDY" &> /dev/null; then
    echo "Error: $CLANG_TIDY not found in PATH."
    exit 1
fi

if [ "$1" == "format" ]; then
    echo "Running formatting engine (in-place) on C++ sources..."
    find src include tests -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) | xargs "$CLANG_FORMAT" -i
    echo "Formatting complete."
    exit 0
elif [ "$1" == "fix" ]; then
    echo "Running static analyzer with auto-fixes..."
    if [ ! -f "build/compile_commands.json" ] && [ ! -f "build-windows/compile_commands.json" ]; then
        echo "Error: compile_commands.json not found. Please run cmake first to generate the compilation database."
        exit 1
    fi
    
    BUILD_DIR="build"
    if [ ! -d "build" ] && [ -d "build-windows" ]; then
        BUILD_DIR="build-windows"
    fi

    EXTRA_ARGS=()
    if [ "$(uname)" == "Darwin" ]; then
        SDK_PATH=$(xcrun --show-sdk-path 2>/dev/null || echo "")
        if [ -n "$SDK_PATH" ]; then
            EXTRA_ARGS+=("-extra-arg=-isysroot" "-extra-arg=$SDK_PATH")
        fi
    fi

    # Pass all compilation units to clang-tidy using the database
    find src tests -type f \( -name "*.cpp" \) | xargs "$CLANG_TIDY" -p "$BUILD_DIR" --fix-errors --format-style=file "${EXTRA_ARGS[@]}"
    echo "Static analysis fixes complete."
    exit 0
fi

echo "Usage: ./lint.sh [format|fix]"
exit 1
