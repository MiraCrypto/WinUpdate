#include "Providers.hpp"
#include "CatUpdateCore.hpp"
#include "HttpClient.hpp"
#include "PlatformTraits.hpp"
#include <cstddef>
#include <exception>
#include <format>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

namespace CatUpdate {

namespace {
std::string GetOsQueryString(PlatformType platform) {
  switch (platform) {
  case PlatformType::Windows:
    return "windows";
  case PlatformType::macOS:
    return "mac";
  case PlatformType::Linux:
    return "linux";
  }
  return "linux";
}
} // namespace

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

bool NodeJsPackageProvider::IsPlatformSupported(PlatformType /*platform*/, ArchitectureType /*arch*/) const {
  return true;
}

std::vector<PackageVersion> NodeJsPackageProvider::FetchAvailableVersions(HttpClient& httpClient) {
  std::vector<PackageVersion> versions;
  const std::string rawJson = FetchRemoteJson(httpClient, "https://nodejs.org/dist/index.json");
  if (rawJson.empty()) {
    return versions;
  }

  try {
    auto parsedData = nlohmann::json::parse(rawJson);
    for (const auto& item : parsedData) {
      versions.push_back(item.value("version", ""));
    }
  } catch (const std::exception& ex) {
    SystemLogger::LogError("Exception parsing Node.js releases JSON", ex.what());
  }
  return versions;
}

UrlString NodeJsPackageProvider::GetDownloadUrl(const PackageVersion& version, PlatformType platform,
                                                ArchitectureType arch) const {
  return std::format("https://nodejs.org/dist/{}/{}", version, GetArchiveFilename(version, platform, arch));
}

std::string NodeJsPackageProvider::GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                                      ArchitectureType arch) const {
  std::string osStr;
  if (platform == PlatformType::Windows) {
    osStr = "win";
  } else if (platform == PlatformType::macOS) {
    osStr = "darwin";
  } else {
    osStr = "linux";
  }

  std::string const archStr = (arch == ArchitectureType::Arm64) ? "arm64" : "x64";
  std::string const ext = PlatformTraits::GetArchiveExtension(platform, arch);
  return std::format("node-{}-{}-{}{}", version, osStr, archStr, ext);
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

bool VSCodiumPackageProvider::IsPlatformSupported(PlatformType /*platform*/, ArchitectureType /*arch*/) const {
  return true;
}

std::vector<PackageVersion> VSCodiumPackageProvider::FetchAvailableVersions(HttpClient& httpClient) {
  std::vector<PackageVersion> versions;
  const std::string rawJson = FetchRemoteJson(httpClient, "https://api.github.com/repos/VSCodium/vscodium/releases");
  if (rawJson.empty()) {
    return versions;
  }

  try {
    auto parsedData = nlohmann::json::parse(rawJson);
    for (const auto& item : parsedData) {
      versions.push_back(item.value("tag_name", ""));
    }
  } catch (const std::exception& ex) {
    SystemLogger::LogError("Exception parsing VSCodium releases JSON", ex.what());
  }
  return versions;
}

UrlString VSCodiumPackageProvider::GetDownloadUrl(const PackageVersion& version, PlatformType platform,
                                                  ArchitectureType arch) const {
  std::string const platformName = (platform == PlatformType::Windows)
                                       ? ("win32-" + std::string(arch == ArchitectureType::Arm64 ? "arm64" : "x64"))
                                       : PlatformTraits::GetPlatformNameString(platform, arch);
  return std::format("https://github.com/VSCodium/vscodium/releases/download/{0}/VSCodium-{1}-{0}{2}", version,
                     platformName, PlatformTraits::GetArchiveExtension(platform, arch));
}

std::string VSCodiumPackageProvider::GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                                        ArchitectureType arch) const {
  std::string const platformName = (platform == PlatformType::Windows)
                                       ? ("win32-" + std::string(arch == ArchitectureType::Arm64 ? "arm64" : "x64"))
                                       : PlatformTraits::GetPlatformNameString(platform, arch);
  return std::format("VSCodium-{}-{}{}", platformName, version, PlatformTraits::GetArchiveExtension(platform, arch));
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

bool PythonPackageProvider::IsPlatformSupported(PlatformType platform, ArchitectureType /*arch*/) const {
  return platform == PlatformType::Windows;
}

std::vector<PackageVersion> PythonPackageProvider::FetchAvailableVersions(HttpClient& /*httpClient*/) {
  return {"3.12.3", "3.11.9", "3.10.11"};
}

UrlString PythonPackageProvider::GetDownloadUrl(const PackageVersion& version, PlatformType /*platform*/,
                                                ArchitectureType arch) const {
  std::string const archStr = (arch == ArchitectureType::Arm64) ? "arm64" : "amd64";
  return std::format("https://www.python.org/ftp/python/{0}/python-{0}-embed-{1}.zip", version, archStr);
}

std::string PythonPackageProvider::GetArchiveFilename(const PackageVersion& version, PlatformType /*platform*/,
                                                      ArchitectureType arch) const {
  std::string const archStr = (arch == ArchitectureType::Arm64) ? "arm64" : "amd64";
  return std::format("python-{}-embed-{}.zip", version, archStr);
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

bool OpenJdkPackageProvider::IsPlatformSupported(PlatformType /*platform*/, ArchitectureType /*arch*/) const {
  return true;
}

std::vector<PackageVersion> OpenJdkPackageProvider::FetchAvailableVersions(HttpClient& httpClient) {
  std::vector<PackageVersion> versions;
  const std::string osQuery = GetOsQueryString(PlatformTraits::GetPlatformType());

  const std::string queryUrl = std::format("https://api.adoptium.net/v3/info/"
                                           "release_names?project=jdk&vendor=eclipse&heap_size=normal&"
                                           "image_type=jdk&architecture=x64&os={}",
                                           osQuery);

  const std::string rawJson = FetchRemoteJson(httpClient, queryUrl);
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
  } catch (const std::exception& ex) {
    SystemLogger::LogError("Exception parsing OpenJDK releases JSON", ex.what());
  }
  return versions;
}

UrlString OpenJdkPackageProvider::GetDownloadUrl(const PackageVersion& version, PlatformType platform,
                                                 ArchitectureType arch) const {
  const std::string osQuery = GetOsQueryString(platform);
  const std::string archQuery = (arch == ArchitectureType::Arm64) ? "aarch64" : "x64";
  return std::format("https://api.adoptium.net/v3/binary/version/{0}/{1}/{2}/jdk/hotspot/normal/eclipse", version,
                     osQuery, archQuery);
}

std::string OpenJdkPackageProvider::GetArchiveFilename(const PackageVersion& version, PlatformType /*platform*/,
                                                       ArchitectureType arch) const {
  const std::string archStr = (arch == ArchitectureType::Arm64) ? "aarch64" : "x64";
  return std::format("openjdk-{}-{}.zip", version, archStr);
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

bool GitPackageProvider::IsPlatformSupported(PlatformType platform, ArchitectureType /*arch*/) const {
  return platform == PlatformType::Windows;
}

std::vector<PackageVersion> GitPackageProvider::FetchAvailableVersions(HttpClient& httpClient) {
  std::vector<PackageVersion> versions;
  const std::string rawJson = FetchRemoteJson(httpClient, "https://api.github.com/repos/git-for-windows/git/releases");
  if (rawJson.empty()) {
    return versions;
  }

  try {
    auto parsedData = nlohmann::json::parse(rawJson);
    for (const auto& item : parsedData) {
      versions.push_back(item.value("tag_name", ""));
    }
  } catch (const std::exception& ex) {
    SystemLogger::LogError("Exception parsing Git releases JSON", ex.what());
  }
  return versions;
}

UrlString GitPackageProvider::GetDownloadUrl(const PackageVersion& version, PlatformType /*platform*/,
                                             ArchitectureType arch) const {
  std::string sanitizedVersion = version;
  if (sanitizedVersion.starts_with("v")) {
    sanitizedVersion = sanitizedVersion.substr(1);
  }
  const size_t winSuffixIndex = sanitizedVersion.find(".windows");
  if (winSuffixIndex != std::string::npos) {
    sanitizedVersion = sanitizedVersion.substr(0, winSuffixIndex);
  }
  std::string const archStr = (arch == ArchitectureType::Arm64) ? "arm64" : "64-bit";
  return std::format("https://github.com/git-for-windows/git/releases/download/{0}/PortableGit-{1}-{2}.7z.exe", version,
                     sanitizedVersion, archStr);
}

std::string GitPackageProvider::GetArchiveFilename(const PackageVersion& version, PlatformType /*platform*/,
                                                   ArchitectureType arch) const {
  std::string const archStr = (arch == ArchitectureType::Arm64) ? "arm64" : "64-bit";
  return std::format("PortableGit-{}-{}.exe", version, archStr);
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

bool VimPackageProvider::IsPlatformSupported(PlatformType platform, ArchitectureType /*arch*/) const {
  return platform == PlatformType::Windows;
}

std::vector<PackageVersion> VimPackageProvider::FetchAvailableVersions(HttpClient& httpClient) {
  std::vector<PackageVersion> versions;
  const std::string rawJson =
      FetchRemoteJson(httpClient, "https://api.github.com/repos/vim/vim-win32-installer/releases");
  if (rawJson.empty()) {
    return versions;
  }

  try {
    auto parsedData = nlohmann::json::parse(rawJson);
    for (const auto& item : parsedData) {
      versions.push_back(item.value("tag_name", ""));
    }
  } catch (const std::exception& ex) {
    SystemLogger::LogError("Exception parsing Vim releases JSON", ex.what());
  }
  return versions;
}

UrlString VimPackageProvider::GetDownloadUrl(const PackageVersion& version, PlatformType /*platform*/,
                                             ArchitectureType arch) const {
  std::string sanitizedVersion = version;
  if (sanitizedVersion.starts_with("v")) {
    sanitizedVersion = sanitizedVersion.substr(1);
  }
  std::string const archStr = (arch == ArchitectureType::Arm64) ? "arm64" : "x64";
  return std::format("https://github.com/vim/vim-win32-installer/releases/download/{0}/gvim_{1}_{2}.zip", version,
                     sanitizedVersion, archStr);
}

std::string VimPackageProvider::GetArchiveFilename(const PackageVersion& version, PlatformType /*platform*/,
                                                   ArchitectureType arch) const {
  std::string const archStr = (arch == ArchitectureType::Arm64) ? "arm64" : "x64";
  return std::format("gvim_{}_{}.zip", version, archStr);
}

// -----------------------------------------------------------------------------
// PackageProviderRegistry Implementation
// -----------------------------------------------------------------------------

std::vector<std::unique_ptr<PackageProvider>> PackageProviderRegistry::GetRegisteredProviders() {
  std::vector<std::unique_ptr<PackageProvider>> providers;

  providers.push_back(std::make_unique<NodeJsPackageProvider>());
  providers.push_back(std::make_unique<VSCodiumPackageProvider>());
  providers.push_back(std::make_unique<PythonPackageProvider>());
  providers.push_back(std::make_unique<OpenJdkPackageProvider>());
  providers.push_back(std::make_unique<GitPackageProvider>());
  providers.push_back(std::make_unique<VimPackageProvider>());

  return providers;
}

} // namespace CatUpdate
