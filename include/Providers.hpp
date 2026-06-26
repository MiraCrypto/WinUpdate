#pragma once
#include "CatUpdateCore.hpp"
#include "HttpClient.hpp"
#include "PlatformTraits.hpp"
#include <memory>
#include <string>
#include <vector>

namespace CatUpdate {

/**
 * Abstract base interface representing a package supplier.
 */
class PackageProvider {
public:
  virtual ~PackageProvider() = default;

  /**
   * Unique string identifier of the software package (e.g. "nodejs").
   */
  virtual PackageIdentifier GetIdentifier() const = 0;

  /**
   * Friendly display name of the software package (e.g. "Node.js (LTS)").
   */
  virtual PackageName GetDisplayName() const = 0;

  /**
   * Returns whether this package can be installed on the requested target operating system and CPU architecture.
   */
  virtual bool IsPlatformSupported(PlatformType platform = PlatformTraits::GetPlatformType(),
                                   ArchitectureType arch = PlatformTraits::GetHostArchitecture()) const = 0;

  /**
   * Fetches the list of available version tags from remote APIs.
   */
  virtual std::vector<PackageVersion>
  FetchAvailableVersions(HttpClient& httpClient, PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                         ArchitectureType targetArch = PlatformTraits::GetHostArchitecture()) = 0;

  /**
   * Computes the direct download URL for a given version and target.
   */
  virtual UrlString GetDownloadUrl(const PackageVersion& version,
                                   PlatformType platform = PlatformTraits::GetPlatformType(),
                                   ArchitectureType arch = PlatformTraits::GetHostArchitecture()) const = 0;

  /**
   * Computes the local archive filename for a given version and target.
   */
  virtual std::string GetArchiveFilename(const PackageVersion& version,
                                         PlatformType platform = PlatformTraits::GetPlatformType(),
                                         ArchitectureType arch = PlatformTraits::GetHostArchitecture()) const = 0;
  /**
   * Returns an optional nested subpath inside the archive containing the actual application binaries.
   * If specified (non-empty), the installer will promote the contents of this subdirectory and discard all other files.
   */
  virtual std::string GetArchiveInternalRoot() const {
    return "";
  }
};

/**
 * Generic base provider supplying common utilities like JSON fetching.
 */
class BasePackageProvider : public PackageProvider {
protected:
  static std::string FetchRemoteJson(HttpClient& httpClient, const UrlString& apiUrl);
};

/**
 * Package provider for Node.js.
 */
class NodeJsPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported(PlatformType platform, ArchitectureType arch) const override;
  std::vector<PackageVersion>
  FetchAvailableVersions(HttpClient& httpClient, PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                         ArchitectureType targetArch = PlatformTraits::GetHostArchitecture()) override;
  UrlString GetDownloadUrl(const PackageVersion& version, PlatformType platform, ArchitectureType arch) const override;
  std::string GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                 ArchitectureType arch) const override;
};

/**
 * Package provider for Node.js (LTS releases only).
 */
class NodeJsLtsPackageProvider : public NodeJsPackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  std::vector<PackageVersion>
  FetchAvailableVersions(HttpClient& httpClient, PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                         ArchitectureType targetArch = PlatformTraits::GetHostArchitecture()) override;
};

/**
 * Package provider for VSCodium.
 */
class VSCodiumPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported(PlatformType platform, ArchitectureType arch) const override;
  std::vector<PackageVersion>
  FetchAvailableVersions(HttpClient& httpClient, PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                         ArchitectureType targetArch = PlatformTraits::GetHostArchitecture()) override;
  UrlString GetDownloadUrl(const PackageVersion& version, PlatformType platform, ArchitectureType arch) const override;
  std::string GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                 ArchitectureType arch) const override;
};

/**
 * Package provider for Visual Studio Code (VSCode).
 */
class VSCodePackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported(PlatformType platform, ArchitectureType arch) const override;
  std::vector<PackageVersion>
  FetchAvailableVersions(HttpClient& httpClient, PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                         ArchitectureType targetArch = PlatformTraits::GetHostArchitecture()) override;
  UrlString GetDownloadUrl(const PackageVersion& version, PlatformType platform, ArchitectureType arch) const override;
  std::string GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                 ArchitectureType arch) const override;
};

/**
 * Package provider for Python (Windows-only via NuGet full binaries).
 */
class PythonPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported(PlatformType platform, ArchitectureType arch) const override;
  std::vector<PackageVersion>
  FetchAvailableVersions(HttpClient& httpClient, PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                         ArchitectureType targetArch = PlatformTraits::GetHostArchitecture()) override;
  UrlString GetDownloadUrl(const PackageVersion& version, PlatformType platform, ArchitectureType arch) const override;
  std::string GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                 ArchitectureType arch) const override;
  std::string GetArchiveInternalRoot() const override;
};

/**
 * Package provider for Eclipse Temurin OpenJDK.
 */
class OpenJdkPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported(PlatformType platform, ArchitectureType arch) const override;
  std::vector<PackageVersion>
  FetchAvailableVersions(HttpClient& httpClient, PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                         ArchitectureType targetArch = PlatformTraits::GetHostArchitecture()) override;
  UrlString GetDownloadUrl(const PackageVersion& version, PlatformType platform, ArchitectureType arch) const override;
  std::string GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                 ArchitectureType arch) const override;
};

/**
 * Package provider for Eclipse Temurin OpenJDK (LTS releases only).
 */
class OpenJdkLtsPackageProvider : public OpenJdkPackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  std::vector<PackageVersion>
  FetchAvailableVersions(HttpClient& httpClient, PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                         ArchitectureType targetArch = PlatformTraits::GetHostArchitecture()) override;
};

/**
 * Package provider for Git for Windows (Windows-only).
 */
class GitPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported(PlatformType platform, ArchitectureType arch) const override;
  std::vector<PackageVersion>
  FetchAvailableVersions(HttpClient& httpClient, PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                         ArchitectureType targetArch = PlatformTraits::GetHostArchitecture()) override;
  UrlString GetDownloadUrl(const PackageVersion& version, PlatformType platform, ArchitectureType arch) const override;
  std::string GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                 ArchitectureType arch) const override;
};

/**
 * Package provider for Vim (portable releases).
 */
class VimPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported(PlatformType platform, ArchitectureType arch) const override;
  std::vector<PackageVersion>
  FetchAvailableVersions(HttpClient& httpClient, PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                         ArchitectureType targetArch = PlatformTraits::GetHostArchitecture()) override;
  UrlString GetDownloadUrl(const PackageVersion& version, PlatformType platform, ArchitectureType arch) const override;
  std::string GetArchiveFilename(const PackageVersion& version, PlatformType platform,
                                 ArchitectureType arch) const override;
};

/**
 * Manages and returns the collection of all package providers.
 */
class PackageProviderRegistry {
public:
  static std::vector<std::unique_ptr<PackageProvider>> GetRegisteredProviders();
};

} // namespace CatUpdate
