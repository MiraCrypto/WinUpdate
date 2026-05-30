#pragma once
#include "WinUpdateCore.hpp"
#include <winhttp.h>
#include <fstream>
#include <format>
#include <sstream>

namespace WinUpdate {

class BaseProvider : public PackageProvider {
protected:
    std::string FetchJson(const std::string& url) {
        std::string result;
        // Reusing Downloader logic for simple JSON fetch
        // In a real app, I'd refactor this into a common HttpClient
        auto tempPath = std::filesystem::temp_directory_path() / "winupdate_temp.json";
        if (Downloader::DownloadFile(url, tempPath, nullptr)) {
            {
                std::ifstream f(tempPath);
                std::stringstream ss;
                ss << f.rdbuf();
                result = ss.str();
            }
            std::error_code ec;
            std::filesystem::remove(tempPath, ec);
        }
        return result;
    }
};

class NodeJsProvider : public BaseProvider {
public:
    std::string GetId() const override { return "nodejs"; }
    std::string GetName() const override { return "Node.js (LTS)"; }
    
    std::vector<std::string> FetchVersions() override {
        std::vector<std::string> versions;
        std::string json = FetchJson("https://nodejs.org/dist/index.json");
        if (!json.empty()) {
            try {
                auto data = nlohmann::json::parse(json);
                for (auto& item : data) {
                    if (item["lts"].is_string()) { // Only LTS
                        versions.push_back(item["version"].get<std::string>());
                    }
                }
            } catch (...) {}
        }
        return versions;
    }

    std::string GetDownloadUrl(const std::string& version) override {
        // v20.11.1 -> https://nodejs.org/dist/v20.11.1/node-v20.11.1-win-x64.zip
        return std::format("https://nodejs.org/dist/{0}/node-{0}-win-x64.zip", version);
    }
};

class VSCodiumProvider : public BaseProvider {
public:
    std::string GetId() const override { return "vscodium"; }
    std::string GetName() const override { return "VSCodium"; }

    std::vector<std::string> FetchVersions() override {
        std::vector<std::string> versions;
        std::string json = FetchJson("https://api.github.com/repos/VSCodium/vscodium/releases");
        if (!json.empty()) {
            try {
                auto data = nlohmann::json::parse(json);
                for (auto& item : data) {
                    versions.push_back(item["tag_name"].get<std::string>());
                }
            } catch (...) {}
        }
        return versions;
    }

    std::string GetDownloadUrl(const std::string& version) override {
        // Pattern: https://github.com/VSCodium/vscodium/releases/download/1.86.2.24046/VSCodium-win32-x64-1.86.2.24046.zip
        return std::format("https://github.com/VSCodium/vscodium/releases/download/{0}/VSCodium-win32-x64-{0}.zip", version);
    }
};

// ... More providers (Python, JDK, Git, Vim) following similar patterns ...

class PythonProvider : public BaseProvider {
public:
    std::string GetId() const override { return "python"; }
    std::string GetName() const override { return "Python (Embeddable)"; }

    std::vector<std::string> FetchVersions() override {
        // Python doesn't have a clean JSON API for embeddable releases easily, 
        // usually scraped from python.org/downloads/windows.
        // For this demo, we'll return common versions.
        return {"3.12.3", "3.11.9", "3.10.11"};
    }

    std::string GetDownloadUrl(const std::string& version) override {
        return std::format("https://www.python.org/ftp/python/{0}/python-{0}-embed-amd64.zip", version);
    }
};

class JdkProvider : public BaseProvider {
public:
    std::string GetId() const override { return "jdk"; }
    std::string GetName() const override { return "OpenJDK (Temurin)"; }

    std::vector<std::string> FetchVersions() override {
        std::vector<std::string> versions;
        std::string json = FetchJson("https://api.adoptium.net/v3/info/release_names?project=jdk&vendor=eclipse&heap_size=normal&image_type=jdk&architecture=x64&os=windows");
        if (!json.empty()) {
            try {
                auto data = nlohmann::json::parse(json);
                for (auto& item : data["releases"]) {
                    versions.push_back(item.get<std::string>());
                }
            } catch (...) {}
        }
        return versions;
    }

    std::string GetDownloadUrl(const std::string& version) override {
        // Simplified: Adoptium has a complex redirect API, but we can use the latest/fixed pattern
        return std::format("https://api.adoptium.net/v3/binary/version/{0}/windows/x64/jdk/hotspot/normal/eclipse", version);
    }
};

class GitProvider : public BaseProvider {
public:
    std::string GetId() const override { return "git"; }
    std::string GetName() const override { return "Git for Windows (Portable)"; }

    std::vector<std::string> FetchVersions() override {
        std::vector<std::string> versions;
        std::string json = FetchJson("https://api.github.com/repos/git-for-windows/git/releases");
        if (!json.empty()) {
            try {
                auto data = nlohmann::json::parse(json);
                for (auto& item : data) {
                    versions.push_back(item["tag_name"].get<std::string>());
                }
            } catch (...) {}
        }
        return versions;
    }

    std::string GetDownloadUrl(const std::string& version) override {
        // v2.44.0.windows.1 -> PortableGit-2.44.0-64-bit.7z.exe
        // Note: Git for Windows uses .7z.exe (self-extracting but tar.exe can handle the 7z inside)
        // Extracting just the version number:
        std::string ver = version;
        if (ver.starts_with("v")) ver = ver.substr(1);
        size_t winIdx = ver.find(".windows");
        if (winIdx != std::string::npos) ver = ver.substr(0, winIdx);

        return std::format("https://github.com/git-for-windows/git/releases/download/{0}/PortableGit-{1}-64-bit.7z.exe", version, ver);
    }
};

class VimProvider : public BaseProvider {
public:
    std::string GetId() const override { return "vim"; }
    std::string GetName() const override { return "Vim"; }

    std::vector<std::string> FetchVersions() override {
        std::vector<std::string> versions;
        std::string json = FetchJson("https://api.github.com/repos/vim/vim-win32-installer/releases");
        if (!json.empty()) {
            try {
                auto data = nlohmann::json::parse(json);
                for (auto& item : data) {
                    versions.push_back(item["tag_name"].get<std::string>());
                }
            } catch (...) {}
        }
        return versions;
    }

    std::string GetDownloadUrl(const std::string& version) override {
        // v9.1.0450 -> gvim_9.1.0450_x64.zip
        std::string ver = version;
        if (ver.starts_with("v")) ver = ver.substr(1);
        return std::format("https://github.com/vim/vim-win32-installer/releases/download/{0}/gvim_{1}_x64.zip", version, ver);
    }
};

} // namespace WinUpdate
