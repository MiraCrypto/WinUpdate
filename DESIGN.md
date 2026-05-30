# WinUpdate: Windows GUI Package Manager Design Document

## 1. Overview
WinUpdate is a lightweight, portable package manager for Windows, designed to install "installation-free" versions of popular development tools into a user-specified directory.

## 2. Architecture
The application follows a modular C++ design:
- **Core (WinUpdateCore):** Logic for fetching versions, downloading binaries, and extraction.
- **GUI (WinUpdateUI):** Native Win32 API implementation for the user interface.
- **Provider (PackageProvider):** Abstract base class for different software sources (GitHub, Node.js API, etc.).
- **Storage (ManifestManager):** Handles tracking of installed versions via a `winupdate.json` file.

## 3. Technologies
- **Language:** C++20 (using modern features like `std::filesystem`, `std::format`).
- **GUI:** Win32 API (Standard controls: ListView, ComboBox, Progress bar).
- **Networking:** `WinHTTP` (Built-in Windows API).
- **Extraction:** `tar.exe` (Built into Windows 10/11, called via `CreateProcess`).
- **JSON:** `nlohmann/json` (Header-only).
- **Cross-Compilation:** `x86_64-w64-mingw32-g++` on Arch Linux.

## 4. Package Definitions & Version Fetching
| Software | Source API / URL | Portable File Pattern |
| :--- | :--- | :--- |
| **VSCodium** | GitHub Releases API | `VSCodium-win32-x64-x.y.z.zip` |
| **Node.js** | `nodejs.org/dist/index.json` | `node-vx.y.z-win-x64.zip` |
| **Python** | Python.org FTP (scraped/pattern) | `python-x.y.z-embed-amd64.zip` |
| **JDK (Temurin)** | `api.adoptium.net` | `OpenJDK-jdk_x64_windows_hotspot_x.y.z.zip` |
| **Git** | GitHub Releases (Git for Windows) | `PortableGit-x.y.z-64-bit.7z.exe` (Use 7z/tar) |
| **Vim** | GitHub Releases (vim-win32-installer) | `gvim_x.x.xxxx_x64.zip` |

## 5. Metadata Tracking
A `winupdate.json` file will be maintained in the root of the installation directory:
```json
{
  "install_path": "C:\\Users\\Public\\Documents\\WinUpdate",
  "packages": [
    {
      "id": "nodejs",
      "version": "20.11.1",
      "install_date": "2026-05-30",
      "status": "installed"
    }
  ]
}
```

## 6. Implementation Plan
1. **Phase 1: Setup & Toolchain** (Makefile/CMake, cross-compilation test).
2. **Phase 2: Networking & JSON** (Fetch version lists for all 6 packages).
3. **Phase 3: Extraction Logic** (Integrate libarchive).
4. **Phase 4: GUI Implementation** (Main window, package list, version selection).
5. **Phase 5: State Persistence** (Manifest tracking).
6. **Phase 6: Refinement** (Error handling, UI polish).
