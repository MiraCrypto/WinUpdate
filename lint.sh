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
    TARGET="native"
    if [ "$2" == "windows" ] || [ "$2" == "win32" ] || [ "$2" == "build-windows" ]; then
        TARGET="windows"
    fi

    # Determine build directory
    BUILD_DIR="build"
    if [ "$TARGET" == "windows" ]; then
        BUILD_DIR="build-windows"
    fi

    if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
        echo "Error: $BUILD_DIR/compile_commands.json not found. Please run cmake first to generate the compilation database."
        exit 1
    fi

    EXTRA_ARGS=()
    # Configure cross-compilation flags if targeting Windows
    if [ "$TARGET" == "windows" ]; then
        MINGW_COMPILER="x86_64-w64-mingw32-g++"
        if command -v "$MINGW_COMPILER" &> /dev/null; then
            SYSROOT=$("$MINGW_COMPILER" -print-sysroot 2>/dev/null || echo "")
            if [ -n "$SYSROOT" ]; then
                EXTRA_ARGS+=("-extra-arg=--target=x86_64-w64-mingw32" "-extra-arg=--sysroot=$SYSROOT")
            fi
        fi
    elif [ "$(uname)" == "Darwin" ]; then
        SDK_PATH=$(xcrun --show-sdk-path 2>/dev/null || echo "")
        if [ -n "$SDK_PATH" ]; then
            EXTRA_ARGS+=("-extra-arg=-isysroot" "-extra-arg=$SDK_PATH")
        fi
    fi

    # Lint files depending on the target configuration
    if [ "$TARGET" == "windows" ]; then
        find src tests -type f \( -name "*.cpp" \) ! -path "src/posix/*" | xargs "$CLANG_TIDY" -p "$BUILD_DIR" --fix-errors --format-style=file "${EXTRA_ARGS[@]}"
    else
        find src tests -type f \( -name "*.cpp" \) ! -path "src/win32/*" ! -name "gui_main.cpp" | xargs "$CLANG_TIDY" -p "$BUILD_DIR" --fix-errors --format-style=file "${EXTRA_ARGS[@]}"
    fi

    echo "Static analysis fixes complete."
    exit 0
fi

echo "Usage: ./lint.sh [format|fix] [build_dir]"
exit 1
