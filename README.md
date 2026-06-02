# Building CatUpdate

This document outlines the instructions required to compile and build the **CatUpdate** targets.

## 1. System Requirements
* **C++ Compiler**: A compiler supporting C++20 standard features (e.g., GCC 11+, Clang 13+, or MSVC 19.29+).
* **Build Systems**: CMake 3.20+ and GNU Make (or MSBuild/Ninja).
* **Dependency Libraries**: `nlohmann/json` (header-only `json.hpp`, automatically downloaded by build scripts if missing).

---

## 2. Build Instructions

### 2.1. Native Build (Linux / macOS)
Compiles the command-line interface (`catupdate`) and the unit tests executable (`catupdate-tests`).

1. **Configure and build with CMake**:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```
2. **Run the Unit Test Suite**:
   ```bash
   ./build/catupdate-tests
   ```

### 2.2. Cross-Compile for Windows (from Linux / macOS)
Compiles the command-line interface (`catupdate.exe`), the Windows-only GUI (`catupdate-gui.exe`), and the unit tests (`catupdate-tests.exe`).

Requires the `x86_64-w64-mingw32-g++` cross-compiler toolchain.

* **Option A**: Run the preconfigured automated build script:
  ```bash
  chmod +x build.sh
  ./build.sh
  ```
* **Option B**: Build manually using CMake:
  ```bash
  cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw64.cmake -DCMAKE_BUILD_TYPE=Release
  cmake --build build-windows
  ```

### 2.3. Native Build on Windows
Compiles the CLI, the GDI Demoscene GUI, and the Unit Tests natively.

Open your terminal (PowerShell, Command Prompt, or Developer PowerShell for VS) and run:
```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
The output executables will be located inside `build/Release/` (if using multi-config MSVC toolchain) or directly under `build/`.
