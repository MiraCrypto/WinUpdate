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
std::string GetOsQueryString() {
  switch (PlatformTraits::GetPlatformType()) {
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

bool NodeJsPackageProvider::IsPlatformSupported() const {
  return true; // Supported on all major platforms
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
      // Restrict to LTS versions only
      if (item.contains("lts") && item["lts"].is_string()) {
        versions.push_back(item.value("version", ""));
      }
    }
  } catch (const std::exception& ex) {
    SystemLogger::LogError("Exception parsing Node.js versions JSON", ex.what());
  }
  return versions;
}

UrlString NodeJsPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
  return std::format("https://nodejs.org/dist/{0}/node-{0}-{1}{2}", version, PlatformTraits::GetPlatformNameString(),
                     PlatformTraits::GetArchiveExtension());
}

std::string NodeJsPackageProvider::GetArchiveFilename(const PackageVersion& version) const {
  return std::format("node-{}-{}{}", version, PlatformTraits::GetPlatformNameString(),
                     PlatformTraits::GetArchiveExtension());
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

UrlString VSCodiumPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
  std::string platformName = (PlatformTraits::GetPlatformType() == PlatformType::Windows)
                                 ? "win32-x64"
                                 : PlatformTraits::GetPlatformNameString();
  return std::format("https://github.com/VSCodium/vscodium/releases/download/{0}/VSCodium-{1}-{0}{2}", version,
                     platformName, PlatformTraits::GetArchiveExtension());
}

std::string VSCodiumPackageProvider::GetArchiveFilename(const PackageVersion& version) const {
  std::string platformName = (PlatformTraits::GetPlatformType() == PlatformType::Windows)
                                 ? "win32-x64"
                                 : PlatformTraits::GetPlatformNameString();
  return std::format("VSCodium-{}-{}{}", platformName, version, PlatformTraits::GetArchiveExtension());
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
  return PlatformTraits::GetPlatformType() == PlatformType::Windows;
}

std::vector<PackageVersion> PythonPackageProvider::FetchAvailableVersions(HttpClient& /*httpClient*/) {
  return {"3.12.3", "3.11.9", "3.10.11"};
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
  const std::string osQuery = GetOsQueryString();

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

UrlString OpenJdkPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
  const std::string osQuery = GetOsQueryString();
  return std::format("https://api.adoptium.net/v3/binary/version/{0}/{1}/x64/jdk/hotspot/normal/eclipse", version,
                     osQuery);
}

std::string OpenJdkPackageProvider::GetArchiveFilename(const PackageVersion& version) const {
  return std::format("openjdk-{}-x64.zip", version);
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
  return PlatformTraits::GetPlatformType() == PlatformType::Windows;
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

UrlString GitPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
  std::string sanitizedVersion = version;
  if (sanitizedVersion.starts_with("v")) {
    sanitizedVersion = sanitizedVersion.substr(1);
  }
  const size_t winSuffixIndex = sanitizedVersion.find(".windows");
  if (winSuffixIndex != std::string::npos) {
    sanitizedVersion = sanitizedVersion.substr(0, winSuffixIndex);
  }
  return std::format("https://github.com/git-for-windows/git/releases/download/{0}/PortableGit-{1}-64-bit.7z.exe",
                     version, sanitizedVersion);
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
  return PlatformTraits::GetPlatformType() == PlatformType::Windows;
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

UrlString VimPackageProvider::GetDownloadUrl(const PackageVersion& version) const {
  std::string sanitizedVersion = version;
  if (sanitizedVersion.starts_with("v")) {
    sanitizedVersion = sanitizedVersion.substr(1);
  }
  return std::format("https://github.com/vim/vim-win32-installer/releases/download/{0}/gvim_{1}_x64.zip", version,
                     sanitizedVersion);
}

std::string VimPackageProvider::GetArchiveFilename(const PackageVersion& version) const {
  return std::format("gvim_{}_x64.zip", version);
}

// -----------------------------------------------------------------------------
// PackageProviderRegistry Implementation
// -----------------------------------------------------------------------------

std::vector<std::unique_ptr<PackageProvider>> PackageProviderRegistry::GetRegisteredProviders() {
  std::vector<std::unique_ptr<PackageProvider>> providers;

  auto nodejs = std::make_unique<NodeJsPackageProvider>();
  auto vscodium = std::make_unique<VSCodiumPackageProvider>();
  auto python = std::make_unique<PythonPackageProvider>();
  auto openjdk = std::make_unique<OpenJdkPackageProvider>();
  auto git = std::make_unique<GitPackageProvider>();
  auto vim = std::make_unique<VimPackageProvider>();

  if (nodejs->IsPlatformSupported()) {
    providers.push_back(std::move(nodejs));
  }
  if (vscodium->IsPlatformSupported()) {
    providers.push_back(std::move(vscodium));
  }
  if (openjdk->IsPlatformSupported()) {
    providers.push_back(std::move(openjdk));
  }
  if (python->IsPlatformSupported()) {
    providers.push_back(std::move(python));
  }
  if (git->IsPlatformSupported()) {
    providers.push_back(std::move(git));
  }
  if (vim->IsPlatformSupported()) {
    providers.push_back(std::move(vim));
  }

  return providers;
}

} // namespace CatUpdate
