#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <windows.h>
#include "json.hpp"

namespace WinUpdate {

struct PackageInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string url;
    std::string filename;
};

struct InstalledPackage {
    std::string id;
    std::string version;
    std::string install_path;
    std::string install_date;
};

class Logger {
public:
    static void Log(const std::wstring& msg);
};

class Utils {
public:
    static std::wstring ToWString(const std::string& str);
    static std::string ToString(const std::wstring& wstr);
};

class Downloader {
public:
    static bool DownloadFile(const std::string& url, const std::filesystem::path& destination, std::function<void(float)> onProgress);
};

class Extractor {
public:
    static bool Extract(const std::filesystem::path& archivePath, const std::filesystem::path& destinationPath);
};

class ManifestManager {
public:
    ManifestManager(const std::filesystem::path& rootPath);
    void Save();
    void UpdatePackage(const InstalledPackage& pkg);
    std::vector<InstalledPackage> GetInstalledPackages() const { return m_installed; }
    std::filesystem::path GetRootPath() const { return m_rootPath; }

private:
    std::filesystem::path m_rootPath;
    std::vector<InstalledPackage> m_installed;
    void Load();
};

class PackageProvider {
public:
    virtual ~PackageProvider() = default;
    virtual std::string GetId() const = 0;
    virtual std::string GetName() const = 0;
    virtual std::vector<std::string> FetchVersions() = 0;
    virtual std::string GetDownloadUrl(const std::string& version) = 0;
};

} // namespace WinUpdate
