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

PackageManager::PackageManager(ManifestManager& manifest) : m_manifest(manifest) {}

bool PackageManager::InstallPackage(PackageProvider& provider, const PackageVersion& version,
                                    const ProgressCallback& progressCallback, const LogCallback& logCallback) {
  auto const url = provider.GetDownloadUrl(version);
  auto const archiveName = provider.GetArchiveFilename(version);
  auto const tempArchiveFile = m_manifest.GetInstallationRootDirectory() / ("temp_" + archiveName);
  auto const targetInstallationDir = m_manifest.GetInstallationRootDirectory() / provider.GetIdentifier();

  if (logCallback) {
    logCallback("--------------------------------------------------");
    logCallback(std::format("Installation target: {} (Version: {})", provider.GetDisplayName(), version));
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
    if (std::filesystem::exists(tempArchiveFile)) {
      std::filesystem::remove(tempArchiveFile);
    }
    return false;
  }

  if (logCallback) {
    logCallback("Download completed successfully.");
    logCallback("Extracting files using native tar.exe...");
  }

  try {
    std::filesystem::create_directories(targetInstallationDir);
  } catch (const std::exception& ex) {
    if (logCallback) {
      logCallback("ERROR: Failed to create installation directory: " + std::string(ex.what()));
    }
    std::filesystem::remove(tempArchiveFile);
    return false;
  }

  auto const extractionCommand = PlatformTraits::GetExtractionCommand(tempArchiveFile, targetInstallationDir);

  auto const extractionResult = ProcessExecutor::ExecuteCommand(extractionCommand);
  std::filesystem::remove(tempArchiveFile);

  if (!extractionResult.has_value() || extractionResult->exitCode != 0) {
    if (logCallback) {
      logCallback("ERROR: Archive extraction failed (tar.exe exit code non-zero).");
      if (extractionResult.has_value()) {
        logCallback("Details: " + extractionResult->standardOutput);
      }
    }
    return false;
  }

  if (logCallback) {
    logCallback("Extraction complete.");
  }

  InstalledPackageState state;
  state.identifier = provider.GetIdentifier();
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
