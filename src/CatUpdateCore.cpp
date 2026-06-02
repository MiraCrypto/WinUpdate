#include "CatUpdateCore.hpp"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <format>
#include <chrono>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace CatUpdate {

// -----------------------------------------------------------------------------
// SystemLogger Implementation
// -----------------------------------------------------------------------------

void SystemLogger::LogInformation(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
#if defined(_WIN32)
    std::wstring wMessage = Utils::ToWString(message);
    OutputDebugStringW((L"[CatUpdate INFO] " + wMessage + L"\n").c_str());
#endif
}

void SystemLogger::LogError(const std::string& message, const std::optional<std::string>& exceptionMessage) {
    std::cerr << "[ERROR] " << message;
    if (exceptionMessage.has_value()) {
        std::cerr << " Details: " << exceptionMessage.value();
    }
    std::cerr << std::endl;
#if defined(_WIN32)
    std::wstring wMessage = Utils::ToWString(message);
    OutputDebugStringW((L"[CatUpdate ERROR] " + wMessage + L"\n").c_str());
#endif
}

// -----------------------------------------------------------------------------
// PathResolver Implementation
// -----------------------------------------------------------------------------

std::filesystem::path PathResolver::GetDefaultInstallationRootPath() {
#if defined(_WIN32)
    WCHAR publicDocumentsPath[MAX_PATH];
    // Retrieve C:\Users\Public\Documents path
    HRESULT result = SHGetFolderPathW(NULL, CSIDL_COMMON_DOCUMENTS, NULL, SHGFP_TYPE_CURRENT, publicDocumentsPath);
    if (SUCCEEDED(result)) {
        return std::filesystem::path(publicDocumentsPath);
    }
    // Fallback to a standard folder if SHGetFolderPathW fails
    return std::filesystem::path("C:\\Users\\Public\\Documents");
#else
    // On Linux/macOS: ~/.local
    return GetUserHomeDirectory() / ".local";
#endif
}

std::filesystem::path PathResolver::GetUserHomeDirectory() {
#if defined(_WIN32)
    WCHAR userProfilePath[MAX_PATH];
    HRESULT result = SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, userProfilePath);
    if (SUCCEEDED(result)) {
        return std::filesystem::path(userProfilePath);
    }
    return std::filesystem::path("C:\\Users\\Default");
#else
    // Fetch HOME environment variable
    const char* homeDirectoryEnv = std::getenv("HOME");
    if (homeDirectoryEnv != nullptr && std::strlen(homeDirectoryEnv) > 0) {
        return std::filesystem::path(homeDirectoryEnv);
    }

    // Fallback via password entries
    struct passwd* passwordEntry = getpwuid(getuid());
    if (passwordEntry != nullptr && passwordEntry->pw_dir != nullptr) {
        return std::filesystem::path(passwordEntry->pw_dir);
    }

    return std::filesystem::path("/");
#endif
}

// -----------------------------------------------------------------------------
// ManifestManager Implementation
// -----------------------------------------------------------------------------

ManifestManager::ManifestManager(const std::filesystem::path& installationRootDirectory)
    : m_installationRootDirectory(installationRootDirectory) {
    LoadManifestFromFile();
}

std::filesystem::path ManifestManager::GetManifestFilePath() const {
    return m_installationRootDirectory / "catupdate.json";
}

std::filesystem::path ManifestManager::GetInstallationRootDirectory() const {
    return m_installationRootDirectory;
}

bool ManifestManager::LoadManifestFromFile() {
    m_installedPackages.clear();
    auto filePath = GetManifestFilePath();
    
    if (!std::filesystem::exists(filePath)) {
        return false;
    }

    std::ifstream fileStream(filePath);
    if (!fileStream.is_open()) {
        SystemLogger::LogError("Failed to open manifest database for reading: " + filePath.string());
        return false;
    }

    try {
        nlohmann::json jsonData = nlohmann::json::parse(fileStream);
        
        if (jsonData.contains("packages") && jsonData["packages"].is_array()) {
            for (const auto& item : jsonData["packages"]) {
                InstalledPackageState state;
                state.identifier = item.value("id", "");
                state.installedVersion = item.value("version", "");
                state.installationPath = std::filesystem::path(item.value("install_path", ""));
                state.installationDate = item.value("install_date", "");
                
                if (!state.identifier.empty() && !state.installedVersion.empty()) {
                    m_installedPackages.push_back(state);
                }
            }
        }
        return true;
    } catch (const std::exception& ex) {
        SystemLogger::LogError("Exception parsing manifest JSON", ex.what());
        return false;
    }
}

bool ManifestManager::SaveManifestToFile() {
    auto filePath = GetManifestFilePath();
    
    // Ensure directory exists
    try {
        std::filesystem::create_directories(m_installationRootDirectory);
    } catch (const std::exception& ex) {
        SystemLogger::LogError("Failed to create root installation directory: " + m_installationRootDirectory.string(), ex.what());
        return false;
    }

    nlohmann::json jsonData;
    jsonData["install_path"] = m_installationRootDirectory.string();
    jsonData["packages"] = nlohmann::json::array();

    for (const auto& pkg : m_installedPackages) {
        nlohmann::json pkgJson = {
            {"id", pkg.identifier},
            {"version", pkg.installedVersion},
            {"install_path", pkg.installationPath.string()},
            {"install_date", pkg.installationDate}
        };
        jsonData["packages"].push_back(pkgJson);
    }

    std::ofstream fileStream(filePath);
    if (!fileStream.is_open()) {
        SystemLogger::LogError("Failed to open manifest database for writing: " + filePath.string());
        return false;
    }

    fileStream << jsonData.dump(4);
    return true;
}

void ManifestManager::RegisterOrUpdateInstalledPackage(const InstalledPackageState& packageState) {
    for (auto& item : m_installedPackages) {
        if (item.identifier == packageState.identifier) {
            item = packageState;
            SaveManifestToFile();
            return;
        }
    }
    m_installedPackages.push_back(packageState);
    SaveManifestToFile();
}

std::optional<InstalledPackageState> ManifestManager::GetInstalledPackageByIdentifier(
    const PackageIdentifier& packageIdentifier
) const {
    for (const auto& item : m_installedPackages) {
        if (item.identifier == packageIdentifier) {
            return item;
        }
    }
    return std::nullopt;
}

bool ManifestManager::IsPackageInstalled(const PackageIdentifier& packageIdentifier) const {
    return GetInstalledPackageByIdentifier(packageIdentifier).has_value();
}

std::vector<InstalledPackageState> ManifestManager::GetInstalledPackages() const {
    return m_installedPackages;
}

// -----------------------------------------------------------------------------
// Utils Implementation
// -----------------------------------------------------------------------------

std::wstring Utils::ToWString(const std::string& utf8Str) {
#if defined(_WIN32)
    if (utf8Str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), static_cast<int>(utf8Str.size()), NULL, 0);
    std::wstring wstrTo(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), static_cast<int>(utf8Str.size()), &wstrTo[0], sizeNeeded);
    return wstrTo;
#else
    return std::wstring(utf8Str.begin(), utf8Str.end());
#endif
}

std::string Utils::ToString(const std::wstring& utf16Str) {
#if defined(_WIN32)
    if (utf16Str.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), static_cast<int>(utf16Str.size()), NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), static_cast<int>(utf16Str.size()), &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
#else
    return std::string(utf16Str.begin(), utf16Str.end());
#endif
}

} // namespace CatUpdate
