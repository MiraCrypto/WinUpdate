#include "Providers.hpp"
#include "CatUpdateCore.hpp"
#include "HttpClient.hpp"
#include "PlatformTraits.hpp"
#include <algorithm>
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

std::string GetAdoptiumArchString(ArchitectureType arch) {
  switch (arch) {
  case ArchitectureType::X64:
    return "x64";
  case ArchitectureType::Arm64:
    return "aarch64";
  }
  return "x64";
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
  return "Node.js";
}

bool NodeJsPackageProvider::IsPlatformSupported(PlatformType /*platform*/, ArchitectureType /*arch*/) const {
  return true;
}

std::vector<PackageVersion> NodeJsPackageProvider::FetchAvailableVersions(HttpClient& httpClient,
                                                                          PlatformType /*targetPlatform*/,
                                                                          ArchitectureType /*targetArch*/) {
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

  std::string const archStr = PlatformTraits::ArchToString(arch);
  std::string const ext = PlatformTraits::GetArchiveExtension(platform, arch);
  return std::format("node-{}-{}-{}{}", version, osStr, archStr, ext);
}

// -----------------------------------------------------------------------------
// Node.js (LTS only) Provider Implementation
// -----------------------------------------------------------------------------

PackageIdentifier NodeJsLtsPackageProvider::GetIdentifier() const {
  return "nodejs-lts";
}

PackageName NodeJsLtsPackageProvider::GetDisplayName() const {
  return "Node.js (LTS)";
}

std::vector<PackageVersion> NodeJsLtsPackageProvider::FetchAvailableVersions(HttpClient& httpClient,
                                                                             PlatformType /*targetPlatform*/,
                                                                             ArchitectureType /*targetArch*/) {
  std::vector<PackageVersion> versions;
  const std::string rawJson = FetchRemoteJson(httpClient, "https://nodejs.org/dist/index.json");
  if (rawJson.empty()) {
    return versions;
  }

  try {
    auto parsedData = nlohmann::json::parse(rawJson);
    for (const auto& item : parsedData) {
      bool isLts = false;
      if (item.contains("lts")) {
        const auto& ltsVal = item["lts"];
        if (ltsVal.is_string() || (ltsVal.is_boolean() && ltsVal.get<bool>())) {
          isLts = true;
        }
      }

      if (isLts) {
        versions.push_back(item.value("version", ""));
      }
    }
  } catch (const std::exception& ex) {
    SystemLogger::LogError("Exception parsing Node.js LTS releases JSON", ex.what());
  }
  return versions;
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

std::vector<PackageVersion> VSCodiumPackageProvider::FetchAvailableVersions(HttpClient& httpClient,
                                                                            PlatformType /*targetPlatform*/,
                                                                            ArchitectureType /*targetArch*/) {
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
                                       ? ("win32-" + PlatformTraits::ArchToString(arch))
                                       : PlatformTraits::GetPlatformNameString(platform, arch);
  return std::format("https://github.com/VSCodium/vscodium/releases/download/{0}/VSCodium-{1}-{0}{2}", version,
                     platformName, PlatformTraits::GetArchiveExtension(platform, arch));
}

std::string VSCodiumPackageProvider::GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                                        ArchitectureType arch) const {
  std::string const platformName = (platform == PlatformType::Windows)
                                       ? ("win32-" + PlatformTraits::ArchToString(arch))
                                       : PlatformTraits::GetPlatformNameString(platform, arch);
  return std::format("VSCodium-{}-{}{}", platformName, version, PlatformTraits::GetArchiveExtension(platform, arch));
}

// -----------------------------------------------------------------------------
// Python Provider Implementation (Windows-only via NuGet)
// -----------------------------------------------------------------------------

PackageIdentifier PythonPackageProvider::GetIdentifier() const {
  return "python";
}

PackageName PythonPackageProvider::GetDisplayName() const {
  return "Python";
}

bool PythonPackageProvider::IsPlatformSupported(PlatformType platform, ArchitectureType /*arch*/) const {
  return platform == PlatformType::Windows;
}

std::vector<PackageVersion> PythonPackageProvider::FetchAvailableVersions(HttpClient& httpClient,
                                                                          PlatformType /*targetPlatform*/,
                                                                          ArchitectureType /*targetArch*/) {
  std::vector<PackageVersion> versions;
  try {
    std::string const queryUrl = "https://api-v2v3search-0.nuget.org/autocomplete?id=python&prerelease=false";
    std::string const response = FetchRemoteJson(httpClient, queryUrl);
    nlohmann::json const json = nlohmann::json::parse(response);

    if (json.contains("data") && json["data"].is_array()) {
      for (const auto& item : json["data"]) {
        versions.push_back(item.get<std::string>());
      }
    }

    std::ranges::reverse(versions);
  } catch (const std::exception& ex) {
    SystemLogger::LogError("Failed to fetch Python versions from NuGet API", ex.what());
  }
  return versions;
}

UrlString PythonPackageProvider::GetDownloadUrl(const PackageVersion& version, PlatformType /*platform*/,
                                                ArchitectureType arch) const {
  std::string const packageId = (arch == ArchitectureType::Arm64) ? "pythonarm64" : "python";
  return std::format("https://www.nuget.org/api/v2/package/{0}/{1}", packageId, version);
}

std::string PythonPackageProvider::GetArchiveFilename(const PackageVersion& version, PlatformType /*platform*/,
                                                      ArchitectureType arch) const {
  std::string const packageId = (arch == ArchitectureType::Arm64) ? "pythonarm64" : "python";
  return std::format("{}-{}.nupkg", packageId, version);
}

std::string PythonPackageProvider::GetArchiveInternalRoot() const {
  return "tools";
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

std::vector<PackageVersion> OpenJdkPackageProvider::FetchAvailableVersions(HttpClient& httpClient,
                                                                           PlatformType targetPlatform,
                                                                           ArchitectureType targetArch) {
  std::vector<PackageVersion> versions;
  const std::string osQuery = GetOsQueryString(targetPlatform);
  const std::string archQuery = GetAdoptiumArchString(targetArch);

  std::string const queryUrl = std::format("https://api.adoptium.net/v3/info/"
                                           "release_names?project=jdk&vendor=eclipse&heap_size=normal&"
                                           "image_type=jdk&release_type=ga&page_size=20&architecture={0}&os={1}",
                                           archQuery, osQuery);

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
  const std::string archQuery = GetAdoptiumArchString(arch);
  return std::format("https://api.adoptium.net/v3/binary/version/{0}/{1}/{2}/jdk/hotspot/normal/eclipse", version,
                     osQuery, archQuery);
}

std::string OpenJdkPackageProvider::GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                                       ArchitectureType arch) const {
  const std::string archStr = GetAdoptiumArchString(arch);
  const std::string ext = (platform == PlatformType::Windows) ? ".zip" : ".tar.gz";
  return std::format("openjdk-{}-{}{}", version, archStr, ext);
}

// -----------------------------------------------------------------------------
// Eclipse Temurin OpenJDK (LTS only) Provider Implementation
// -----------------------------------------------------------------------------

PackageIdentifier OpenJdkLtsPackageProvider::GetIdentifier() const {
  return "jdk-lts";
}

PackageName OpenJdkLtsPackageProvider::GetDisplayName() const {
  return "OpenJDK (Temurin) (LTS)";
}

std::vector<PackageVersion> OpenJdkLtsPackageProvider::FetchAvailableVersions(HttpClient& httpClient,
                                                                              PlatformType targetPlatform,
                                                                              ArchitectureType targetArch) {
  std::vector<PackageVersion> versions;
  const std::string osQuery = GetOsQueryString(targetPlatform);
  const std::string archQuery = GetAdoptiumArchString(targetArch);

  // Query with &lts=true to fetch ONLY stable LTS releases from Adoptium!
  std::string const queryUrl =
      std::format("https://api.adoptium.net/v3/info/"
                  "release_names?project=jdk&vendor=eclipse&heap_size=normal&"
                  "image_type=jdk&release_type=ga&lts=true&page_size=20&architecture={0}&os={1}",
                  archQuery, osQuery);

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
    SystemLogger::LogError("Exception parsing OpenJDK LTS releases JSON", ex.what());
  }
  return versions;
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

std::vector<PackageVersion> GitPackageProvider::FetchAvailableVersions(HttpClient& httpClient,
                                                                       PlatformType /*targetPlatform*/,
                                                                       ArchitectureType /*targetArch*/) {
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

std::vector<PackageVersion> VimPackageProvider::FetchAvailableVersions(HttpClient& httpClient,
                                                                       PlatformType /*targetPlatform*/,
                                                                       ArchitectureType /*targetArch*/) {
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
  std::string const archStr = PlatformTraits::ArchToString(arch);
  return std::format("https://github.com/vim/vim-win32-installer/releases/download/{0}/gvim_{1}_{2}.zip", version,
                     sanitizedVersion, archStr);
}

std::string VimPackageProvider::GetArchiveFilename(const PackageVersion& version, PlatformType /*platform*/,
                                                   ArchitectureType arch) const {
  std::string const archStr = PlatformTraits::ArchToString(arch);
  return std::format("gvim_{}_{}.zip", version, archStr);
}

// -----------------------------------------------------------------------------
// PackageProviderRegistry Implementation
// -----------------------------------------------------------------------------

std::vector<std::unique_ptr<PackageProvider>> PackageProviderRegistry::GetRegisteredProviders() {
  std::vector<std::unique_ptr<PackageProvider>> providers;

  providers.push_back(std::make_unique<NodeJsPackageProvider>());
  providers.push_back(std::make_unique<NodeJsLtsPackageProvider>());
  providers.push_back(std::make_unique<VSCodiumPackageProvider>());
  providers.push_back(std::make_unique<PythonPackageProvider>());
  providers.push_back(std::make_unique<OpenJdkPackageProvider>());
  providers.push_back(std::make_unique<OpenJdkLtsPackageProvider>());
  providers.push_back(std::make_unique<GitPackageProvider>());
  providers.push_back(std::make_unique<VimPackageProvider>());

  return providers;
}

} // namespace CatUpdate
