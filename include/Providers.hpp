#pragma once

#include "CatUpdateCore.hpp"
#include "HttpClient.hpp"
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
   * Returns whether this package can be installed on the current running operating system.
   */
  virtual bool IsPlatformSupported() const = 0;

  /**
   * Fetches the list of available version tags from remote APIs.
   */
  virtual std::vector<PackageVersion> FetchAvailableVersions(HttpClient& httpClient) = 0;

  /**
   * Computes the direct download URL for a given version.
   */
  virtual UrlString GetDownloadUrl(const PackageVersion& version) const = 0;

  /**
   * Computes the local archive filename for a given version.
   */
  virtual std::string GetArchiveFilename(const PackageVersion& version) const = 0;
};

/**
 * Generic base provider supplying common utilities like JSON fetching.
 */
class BasePackageProvider : public PackageProvider {
protected:
  static std::string FetchRemoteJson(HttpClient& httpClient, const UrlString& apiUrl);
};

/**
 * Package provider for Node.js (LTS releases).
 */
class NodeJsPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported() const override;
  std::vector<PackageVersion> FetchAvailableVersions(HttpClient& httpClient) override;
  UrlString GetDownloadUrl(const PackageVersion& version) const override;
  std::string GetArchiveFilename(const PackageVersion& version) const override;
};

/**
 * Package provider for VSCodium.
 */
class VSCodiumPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported() const override;
  std::vector<PackageVersion> FetchAvailableVersions(HttpClient& httpClient) override;
  UrlString GetDownloadUrl(const PackageVersion& version) const override;
  std::string GetArchiveFilename(const PackageVersion& version) const override;
};

/**
 * Package provider for Embeddable Python (Windows-only).
 */
class PythonPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported() const override;
  std::vector<PackageVersion> FetchAvailableVersions(HttpClient& httpClient) override;
  UrlString GetDownloadUrl(const PackageVersion& version) const override;
  std::string GetArchiveFilename(const PackageVersion& version) const override;
};

/**
 * Package provider for Eclipse Temurin OpenJDK.
 */
class OpenJdkPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported() const override;
  std::vector<PackageVersion> FetchAvailableVersions(HttpClient& httpClient) override;
  UrlString GetDownloadUrl(const PackageVersion& version) const override;
  std::string GetArchiveFilename(const PackageVersion& version) const override;
};

/**
 * Package provider for Git for Windows (Windows-only).
 */
class GitPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported() const override;
  std::vector<PackageVersion> FetchAvailableVersions(HttpClient& httpClient) override;
  UrlString GetDownloadUrl(const PackageVersion& version) const override;
  std::string GetArchiveFilename(const PackageVersion& version) const override;
};

/**
 * Package provider for Vim (portable releases).
 */
class VimPackageProvider : public BasePackageProvider {
public:
  PackageIdentifier GetIdentifier() const override;
  PackageName GetDisplayName() const override;
  bool IsPlatformSupported() const override;
  std::vector<PackageVersion> FetchAvailableVersions(HttpClient& httpClient) override;
  UrlString GetDownloadUrl(const PackageVersion& version) const override;
  std::string GetArchiveFilename(const PackageVersion& version) const override;
};

/**
 * Manages and returns the collection of providers supported on the current platform.
 */
class PackageProviderRegistry {
public:
  static std::vector<std::unique_ptr<PackageProvider>> GetRegisteredProviders();
};

} // namespace CatUpdate
