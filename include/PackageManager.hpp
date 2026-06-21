#pragma once

#include "CatUpdateCore.hpp"
#include "Providers.hpp"
#include <functional>
#include <string>
#include <vector>

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

private:
  ManifestManager& m_manifest;
};

} // namespace CatUpdate
