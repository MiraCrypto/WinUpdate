#include "PackageManager.hpp"
#include "CatUpdateCore.hpp"
#include "HttpClient.hpp"
#include "PlatformTraits.hpp"
#include "ProcessExecutor.hpp"
#include "Providers.hpp"
#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <format>
#include <utility>

namespace CatUpdate {

namespace {
bool CheckPlatformConflict(const ManifestManager& manifest, const PackageIdentifier& packageId,
                           const std::string& targetPlatformStr, const std::string& targetArchStr,
                           const LogCallback& logCallback, const PackageName& displayName) {
  auto const existingInstall = manifest.GetInstalledPackageByIdentifier(packageId);
  if (existingInstall.has_value()) {
    if (existingInstall->targetPlatform != targetPlatformStr || existingInstall->targetArchitecture != targetArchStr) {
      if (logCallback) {
        logCallback("--------------------------------------------------");
        logCallback(std::format("ERROR: Platform conflict. {} is already installed targeting '{}-{}'.", displayName,
                                existingInstall->targetPlatform, existingInstall->targetArchitecture));
        logCallback(std::format("Requested target: '{}-{}'.", targetPlatformStr, targetArchStr));
        logCallback("Please uninstall the existing package first to change the target platform/architecture.");
      }
      return false;
    }
  }
  return true;
}

void FlattenInstallationDirectory(const std::filesystem::path& directory) {
  if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
    return;
  }

  size_t childCount = 0;
  std::filesystem::path singleSubdir;

  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    childCount++;
    if (std::filesystem::is_directory(entry.path())) {
      singleSubdir = entry.path();
    }
  }

  // If there is exactly one child and it is a directory, promote its contents
  if (childCount == 1 && !singleSubdir.empty()) {
    std::vector<std::filesystem::path> pathsToMove;
    for (const auto& entry : std::filesystem::directory_iterator(singleSubdir)) {
      pathsToMove.push_back(entry.path());
    }

    for (const auto& path : pathsToMove) {
      std::filesystem::path const destPath = directory / path.filename();
      std::filesystem::rename(path, destPath);
    }

    // Remove the now-empty nested directory
    std::filesystem::remove(singleSubdir);
  }
}

bool SwapDirectoriesWithRollback(const std::filesystem::path& tempExtractDir,
                                 const std::filesystem::path& targetInstallationDir,
                                 const std::filesystem::path& oldBackupDir, const LogCallback& logCallback) {
  // Clean up old backup directory if exists (best effort from previous runs)
  if (std::filesystem::exists(oldBackupDir)) {
    try {
      std::filesystem::remove_all(oldBackupDir);
    } catch (const std::exception& ex) {
      SystemLogger::LogInformation("Warning: Failed to clean up stale backup directory: " + std::string(ex.what()));
    }
  }

  try {
    if (std::filesystem::exists(targetInstallationDir)) {
      std::filesystem::rename(targetInstallationDir, oldBackupDir);
    }
    std::filesystem::rename(tempExtractDir, targetInstallationDir);
  } catch (const std::exception& ex) {
    if (logCallback) {
      logCallback("ERROR: Failed to swap installation directories: " + std::string(ex.what()));
    }
    // Rollback
    if (std::filesystem::exists(oldBackupDir) && !std::filesystem::exists(targetInstallationDir)) {
      try {
        std::filesystem::rename(oldBackupDir, targetInstallationDir);
      } catch (const std::exception& rollbackEx) {
        SystemLogger::LogError("Critical: Failed to roll back installation directory swap: " +
                               std::string(rollbackEx.what()));
      }
    }
    std::filesystem::remove_all(tempExtractDir);
    return false;
  }

  // Clean up old backup directory (best effort)
  try {
    if (std::filesystem::exists(oldBackupDir)) {
      std::filesystem::remove_all(oldBackupDir);
    }
  } catch (const std::exception& cleanupEx) {
    SystemLogger::LogInformation("Warning: Stale backup directory cleanup deferred: " + std::string(cleanupEx.what()));
  }

  return true;
}
} // namespace

PackageManager::PackageManager(ManifestManager& manifest) : m_manifest(manifest) {}

bool PackageManager::InstallPackage(PackageProvider& provider, const PackageVersion& version,
                                    PlatformType targetPlatform, ArchitectureType targetArch,
                                    const ProgressCallback& progressCallback, const LogCallback& logCallback) {
  const std::string targetPlatformStr = PlatformTraits::PlatformToString(targetPlatform);
  const std::string targetArchStr = PlatformTraits::ArchToString(targetArch);

  // Check if the package is already installed and detect target platform/arch conflicts
  if (!CheckPlatformConflict(m_manifest, provider.GetIdentifier(), targetPlatformStr, targetArchStr, logCallback,
                             provider.GetDisplayName())) {
    return false;
  }

  auto const url = provider.GetDownloadUrl(version, targetPlatform, targetArch);
  auto const archiveName = provider.GetArchiveFilename(version, targetPlatform, targetArch);
  auto const tempArchiveFile = m_manifest.GetInstallationRootDirectory() / ("temp_" + archiveName);
  auto const tempExtractDir = m_manifest.GetInstallationRootDirectory() / ("temp_extract_" + provider.GetIdentifier());
  auto const targetInstallationDir = m_manifest.GetInstallationRootDirectory() / provider.GetIdentifier();

  // 1. Clean up any leftover temporary files/folders from previous crashed runs
  std::filesystem::remove(tempArchiveFile);
  std::filesystem::remove_all(tempExtractDir);

  if (logCallback) {
    logCallback("--------------------------------------------------");
    logCallback(std::format("Installation target: {} (Version: {}, Target: {}-{})", provider.GetDisplayName(), version,
                            targetPlatformStr, targetArchStr));
    logCallback("Source URL: " + url);
  }

  try {
    std::filesystem::create_directories(m_manifest.GetInstallationRootDirectory());
  } catch (const std::exception& ex) {
    if (logCallback) {
      logCallback("ERROR: Failed to create root installation directory: " + std::string(ex.what()));
    }
    return false;
  }

  auto httpClient = HttpClientFactory::CreateDefaultClient();
  bool const downloadSuccess = httpClient->DownloadFile(url, tempArchiveFile, progressCallback);

  if (!downloadSuccess) {
    if (logCallback) {
      logCallback("ERROR: Download request failed.");
    }
    std::filesystem::remove(tempArchiveFile);
    return false;
  }

  if (logCallback) {
    logCallback("Download completed successfully.");
    logCallback("Extracting files...");
  }

  // 2. Create the temporary sandbox directory
  try {
    std::filesystem::create_directories(tempExtractDir);
  } catch (const std::exception& ex) {
    if (logCallback) {
      logCallback("ERROR: Failed to create sandbox directory: " + std::string(ex.what()));
    }
    std::filesystem::remove(tempArchiveFile);
    return false;
  }

  // 3. Extract into the sandbox
  auto const extractionCommand = PlatformTraits::GetExtractionCommand(tempArchiveFile, tempExtractDir);
  auto const extractionResult = ProcessExecutor::ExecuteCommand(extractionCommand);
  std::filesystem::remove(tempArchiveFile);

  if (!extractionResult.has_value() || extractionResult->exitCode != 0) {
    if (logCallback) {
      logCallback("ERROR: Archive extraction failed (exit code non-zero).");
      if (extractionResult.has_value()) {
        logCallback("Details: " + extractionResult->standardOutput);
      }
    }
    std::filesystem::remove_all(tempExtractDir);
    return false;
  }

  if (logCallback) {
    logCallback("Extraction complete.");
  }

  // 4. Flatten the directory inside the sandbox
  FlattenInstallationDirectory(tempExtractDir);

  // 5. Atomic swap with rollback protection
  auto const oldBackupDir = m_manifest.GetInstallationRootDirectory() / ("old_" + provider.GetIdentifier());
  if (!SwapDirectoriesWithRollback(tempExtractDir, targetInstallationDir, oldBackupDir, logCallback)) {
    return false;
  }

  InstalledPackageState state;
  state.identifier = provider.GetIdentifier();
  state.targetPlatform = targetPlatformStr;
  state.targetArchitecture = targetArchStr;
  state.installedVersion = version;
  state.installationPath = targetInstallationDir;

  auto const now = std::chrono::system_clock::now();
  auto const nowTime = std::chrono::system_clock::to_time_t(now);
  std::tm timeStruct = *std::localtime(&nowTime);
  state.installationDate =
      std::format("{:04}-{:02}-{:02}", timeStruct.tm_year + 1900, timeStruct.tm_mon + 1, timeStruct.tm_mday);

  m_manifest.RegisterOrUpdateInstalledPackage(state);

  if (logCallback) {
    logCallback(std::format("Success: {} successfully installed at {}", provider.GetDisplayName(),
                            targetInstallationDir.string()));
  }

  return true;
}

bool PackageManager::UninstallPackage(const PackageIdentifier& packageId, const LogCallback& logCallback) {
  if (logCallback) {
    logCallback("--------------------------------------------------");
  }

  auto const packageState = m_manifest.GetInstalledPackageByIdentifier(packageId);
  if (!packageState.has_value()) {
    if (logCallback) {
      logCallback("ERROR: Package is not registered as installed.");
    }
    return false;
  }

  if (logCallback) {
    logCallback("Uninstalling package: " + packageId);
    logCallback("Removing installation files at: " + packageState->installationPath.string());
  }

  try {
    if (std::filesystem::exists(packageState->installationPath)) {
      std::filesystem::remove_all(packageState->installationPath);
    }

    m_manifest.UnregisterInstalledPackage(packageId);

    if (logCallback) {
      logCallback("Files removed successfully.");
      logCallback(std::format("Success: {} uninstalled successfully.", packageId));
    }
    return true;
  } catch (const std::exception& ex) {
    if (logCallback) {
      logCallback("ERROR: Failed to uninstall package: " + std::string(ex.what()));
    }
    return false;
  }
}

} // namespace CatUpdate
