#pragma once

#include "CatUpdateCore.hpp"
#include "Providers.hpp"
#include <functional>
#include <string>
#include <vector>

#include <map>
#include <mutex>
#include <set>

namespace CatUpdate {

using ProgressCallback = std::function<void(float progress)>;
using LogCallback = std::function<void(const std::string& message)>;

/**
 * High-level orchestration manager for package workflows.
 * Unifies the installation and uninstallation logic for CLI and GUI.
 */
class PackageManager {
public:
  explicit PackageManager(ManifestManager& manifest);

  bool InstallPackage(PackageProvider& provider, const PackageVersion& version,
                      PlatformType targetPlatform = PlatformTraits::GetPlatformType(),
                      ArchitectureType targetArch = PlatformTraits::GetHostArchitecture(),
                      const ProgressCallback& progressCallback = nullptr, const LogCallback& logCallback = nullptr);

  bool UninstallPackage(const PackageIdentifier& packageId, const LogCallback& logCallback = nullptr);

  enum class UpdateStatus { NotInstalled, Checking, UpToDate, UpdateAvailable, CheckFailed };

  UpdateStatus GetUpdateStatus(PackageProvider& provider, std::string& outLatestVersion,
                               const std::function<void(const std::vector<PackageVersion>&)>& onComplete = nullptr);

  std::vector<PackageVersion> GetCachedVersions(const PackageIdentifier& packageId);

  void ClearVersionsCache();

private:
  ManifestManager& m_manifest;
  std::map<PackageIdentifier, std::vector<PackageVersion>> m_versionsCache;
  std::set<PackageIdentifier> m_pendingVersionQueries;
  std::mutex m_cacheMutex;
};

} // namespace CatUpdate
