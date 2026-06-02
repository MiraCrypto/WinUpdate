#include "Providers.hpp"
#include "json.hpp"
#include <format>
#include <algorithm>

namespace CatUpdate {

// Helper to perform remote fetches and parse them safely
std::string BasePackageProvider::FetchRemoteJson(HttpClient& httpClient, const UrlString& apiUrl) {
    try {
        return httpClient.FetchStringContent(apiUrl);
    } catch (...) {
        return "";
    }
}

// -----------------------------------------------------------------------------
// Node.js Provider Implementation
// -----------------------------------------------------------------------------

PackageIdentifier NodeJsPackageProvider::GetIdentifier() const {
    return "nodejs";
}

PackageName NodeJsPackageProvider::GetDisplayName() const {
    return "Node.js (LTS)";
}

bool NodeJsPackageProvider::IsPlatformSupported() const {
    return true; // Supported on all major platforms
}

std::vector<PackageVersion> NodeJsPackageProvider::FetchAvailableVersions(HttpClient& httpClient) {
    std::vector<PackageVersion> versions;
    std::string rawJson = FetchRemoteJson(httpClient, "https://nodejs.org/dist/index.json");
    if (rawJson.empty()) {
        return versions;
    }

    try {
        auto parsedData = nlohmann::json::parse(rawJson);
        for (const auto& item : parsedData) {
            // Restrict to LTS versions only
            if (item.contains("lts") && item["lts"].is_string()) {
                versions.push_back(item.value("version", ""));
            }
        }
    } catch (...) {
        // Safely ignore parse exceptions and return empty/partial results
    }
    return versions;
}

UrlString NodeJsPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
#if defined(_WIN32)
    return std::format("https://nodejs.org/dist/{0}/node-{0}-win-x64.zip", version);
#elif defined(__APPLE__)
    return std::format("https://nodejs.org/dist/{0}/node-{0}-darwin-x64.tar.gz", version);
#else
    return std::format("https://nodejs.org/dist/{0}/node-{0}-linux-x64.tar.xz", version);
#endif
}

std::string NodeJsPackageProvider::GetArchiveFilename(const PackageVersion& version) const {
#if defined(_WIN32)
    return std::format("node-{}-win-x64.zip", version);
#elif defined(__APPLE__)
    return std::format("node-{}-darwin-x64.tar.gz", version);
#else
    return std::format("node-{}-linux-x64.tar.xz", version);
#endif
}

// -----------------------------------------------------------------------------
// VSCodium Provider Implementation
// -----------------------------------------------------------------------------

PackageIdentifier VSCodiumPackageProvider::GetIdentifier() const {
    return "vscodium";
}

PackageName VSCodiumPackageProvider::GetDisplayName() const {
    return "VSCodium";
}

bool VSCodiumPackageProvider::IsPlatformSupported() const {
    return true; // Supported on all major platforms
}

std::vector<PackageVersion> VSCodiumPackageProvider::FetchAvailableVersions(HttpClient& httpClient) {
    std::vector<PackageVersion> versions;
    std::string rawJson = FetchRemoteJson(httpClient, "https://api.github.com/repos/VSCodium/vscodium/releases");
    if (rawJson.empty()) {
        return versions;
    }

    try {
        auto parsedData = nlohmann::json::parse(rawJson);
        for (const auto& item : parsedData) {
            versions.push_back(item.value("tag_name", ""));
        }
    } catch (...) {
    }
    return versions;
}

UrlString VSCodiumPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
#if defined(_WIN32)
    return std::format("https://github.com/VSCodium/vscodium/releases/download/{0}/VSCodium-win32-x64-{0}.zip", version);
#elif defined(__APPLE__)
    return std::format("https://github.com/VSCodium/vscodium/releases/download/{0}/VSCodium-darwin-x64-{0}.zip", version);
#else
    return std::format("https://github.com/VSCodium/vscodium/releases/download/{0}/VSCodium-linux-x64-{0}.tar.gz", version);
#endif
}

std::string VSCodiumPackageProvider::GetArchiveFilename(const PackageVersion& version) const {
#if defined(_WIN32)
    return std::format("VSCodium-win32-x64-{}.zip", version);
#elif defined(__APPLE__)
    return std::format("VSCodium-darwin-x64-{}.zip", version);
#else
    return std::format("VSCodium-linux-x64-{}.tar.gz", version);
#endif
}

// -----------------------------------------------------------------------------
// Python Provider Implementation (Windows-only)
// -----------------------------------------------------------------------------

PackageIdentifier PythonPackageProvider::GetIdentifier() const {
    return "python";
}

PackageName PythonPackageProvider::GetDisplayName() const {
    return "Python (Embeddable)";
}

bool PythonPackageProvider::IsPlatformSupported() const {
#if defined(_WIN32)
    return true;
#else
    return false; // On Linux/macOS, standard Python is preferred
#endif
}

std::vector<PackageVersion> PythonPackageProvider::FetchAvailableVersions(HttpClient& /*httpClient*/) {
    // Embeddable Python releases do not have a clean standalone JSON API.
    // Returning standard active versions.
    return { "3.12.3", "3.11.9", "3.10.11" };
}

UrlString PythonPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
    return std::format("https://www.python.org/ftp/python/{0}/python-{0}-embed-amd64.zip", version);
}

std::string PythonPackageProvider::GetArchiveFilename(const PackageVersion& version) const {
    return std::format("python-{}-embed-amd64.zip", version);
}

// -----------------------------------------------------------------------------
// Eclipse Temurin OpenJDK Provider Implementation
// -----------------------------------------------------------------------------

PackageIdentifier OpenJdkPackageProvider::GetIdentifier() const {
    return "jdk";
}

PackageName OpenJdkPackageProvider::GetDisplayName() const {
    return "OpenJDK (Temurin)";
}

bool OpenJdkPackageProvider::IsPlatformSupported() const {
    return true;
}

std::vector<PackageVersion> OpenJdkPackageProvider::FetchAvailableVersions(HttpClient& httpClient) {
    std::vector<PackageVersion> versions;
#if defined(_WIN32)
    std::string queryUrl = "https://api.adoptium.net/v3/info/release_names?project=jdk&vendor=eclipse&heap_size=normal&image_type=jdk&architecture=x64&os=windows";
#elif defined(__APPLE__)
    std::string queryUrl = "https://api.adoptium.net/v3/info/release_names?project=jdk&vendor=eclipse&heap_size=normal&image_type=jdk&architecture=x64&os=mac";
#else
    std::string queryUrl = "https://api.adoptium.net/v3/info/release_names?project=jdk&vendor=eclipse&heap_size=normal&image_type=jdk&architecture=x64&os=linux";
#endif

    std::string rawJson = FetchRemoteJson(httpClient, queryUrl);
    if (rawJson.empty()) {
        return versions;
    }

    try {
        auto parsedData = nlohmann::json::parse(rawJson);
        if (parsedData.contains("releases") && parsedData["releases"].is_array()) {
            for (const auto& item : parsedData["releases"]) {
                versions.push_back(item.get<std::string>());
            }
        }
    } catch (...) {
    }
    return versions;
}

UrlString OpenJdkPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
#if defined(_WIN32)
    return std::format("https://api.adoptium.net/v3/binary/version/{0}/windows/x64/jdk/hotspot/normal/eclipse", version);
#elif defined(__APPLE__)
    return std::format("https://api.adoptium.net/v3/binary/version/{0}/mac/x64/jdk/hotspot/normal/eclipse", version);
#else
    return std::format("https://api.adoptium.net/v3/binary/version/{0}/linux/x64/jdk/hotspot/normal/eclipse", version);
#endif
}

std::string OpenJdkPackageProvider::GetArchiveFilename(const PackageVersion& version) const {
    return std::format("openjdk-{}-x64.zip", version); // tar.gz on POSIX but zip wrapper matches Adoptium API response
}

// -----------------------------------------------------------------------------
// Git Provider Implementation (Windows-only)
// -----------------------------------------------------------------------------

PackageIdentifier GitPackageProvider::GetIdentifier() const {
    return "git";
}

PackageName GitPackageProvider::GetDisplayName() const {
    return "Git for Windows (Portable)";
}

bool GitPackageProvider::IsPlatformSupported() const {
#if defined(_WIN32)
    return true;
#else
    return false; // Linux has standard git via apt/yum/pacman
#endif
}

std::vector<PackageVersion> GitPackageProvider::FetchAvailableVersions(HttpClient& httpClient) {
    std::vector<PackageVersion> versions;
    std::string rawJson = FetchRemoteJson(httpClient, "https://api.github.com/repos/git-for-windows/git/releases");
    if (rawJson.empty()) {
        return versions;
    }

    try {
        auto parsedData = nlohmann::json::parse(rawJson);
        for (const auto& item : parsedData) {
            versions.push_back(item.value("tag_name", ""));
        }
    } catch (...) {
    }
    return versions;
}

UrlString GitPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
    std::string ver = version;
    if (ver.starts_with("v")) {
        ver = ver.substr(1);
    }
    size_t winSuffixIndex = ver.find(".windows");
    if (winSuffixIndex != std::string::npos) {
        ver = ver.substr(0, winSuffixIndex);
    }
    return std::format("https://github.com/git-for-windows/git/releases/download/{0}/PortableGit-{1}-64-bit.7z.exe", version, ver);
}

std::string GitPackageProvider::GetArchiveFilename(const PackageVersion& version) const {
    return std::format("PortableGit-{}-64-bit.exe", version);
}

// -----------------------------------------------------------------------------
// Vim Provider Implementation (Windows-only for now)
// -----------------------------------------------------------------------------

PackageIdentifier VimPackageProvider::GetIdentifier() const {
    return "vim";
}

PackageName VimPackageProvider::GetDisplayName() const {
    return "Vim (Portable)";
}

bool VimPackageProvider::IsPlatformSupported() const {
#if defined(_WIN32)
    return true;
#else
    return false; // Unix systems have vim pre-installed or via standard system channels
#endif
}

std::vector<PackageVersion> VimPackageProvider::FetchAvailableVersions(HttpClient& httpClient) {
    std::vector<PackageVersion> versions;
    std::string rawJson = FetchRemoteJson(httpClient, "https://api.github.com/repos/vim/vim-win32-installer/releases");
    if (rawJson.empty()) {
        return versions;
    }

    try {
        auto parsedData = nlohmann::json::parse(rawJson);
        for (const auto& item : parsedData) {
            versions.push_back(item.value("tag_name", ""));
        }
    } catch (...) {
    }
    return versions;
}

UrlString VimPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
    std::string ver = version;
    if (ver.starts_with("v")) {
        ver = ver.substr(1);
    }
    return std::format("https://github.com/vim/vim-win32-installer/releases/download/{0}/gvim_{1}_x64.zip", version, ver);
}

std::string VimPackageProvider::GetArchiveFilename(const PackageVersion& version) const {
    return std::format("gvim_{}_x64.zip", version);
}

// -----------------------------------------------------------------------------
// PackageProviderRegistry Implementation
// -----------------------------------------------------------------------------

std::vector<std::unique_ptr<PackageProvider>> PackageProviderRegistry::GetRegisteredProviders() {
    std::vector<std::unique_ptr<PackageProvider>> providers;
    
    // Instantiate all potential providers
    auto nodejs = std::make_unique<NodeJsPackageProvider>();
    auto vscodium = std::make_unique<VSCodiumPackageProvider>();
    auto python = std::make_unique<PythonPackageProvider>();
    auto openjdk = std::make_unique<OpenJdkPackageProvider>();
    auto git = std::make_unique<GitPackageProvider>();
    auto vim = std::make_unique<VimPackageProvider>();

    // Check host compatibility before registering
    if (nodejs->IsPlatformSupported()) providers.push_back(std::move(nodejs));
    if (vscodium->IsPlatformSupported()) providers.push_back(std::move(vscodium));
    if (openjdk->IsPlatformSupported()) providers.push_back(std::move(openjdk));
    if (python->IsPlatformSupported()) providers.push_back(std::move(python));
    if (git->IsPlatformSupported()) providers.push_back(std::move(git));
    if (vim->IsPlatformSupported()) providers.push_back(std::move(vim));

    return providers;
}

} // namespace CatUpdate
