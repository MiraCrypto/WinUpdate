#include "CatUpdateCore.hpp"
#include "PlatformTraits.hpp"
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace CatUpdate {

namespace {
InstalledPackageState ParsePackageState(const nlohmann::json& item) {
  InstalledPackageState state;
  state.identifier = item.value("id", "");
  state.installedVersion = item.value("version", "");
  state.installationPath = std::filesystem::path(item.value("install_path", ""));
  state.installationDate = item.value("install_date", "");

  state.targetPlatform = item.value("target_platform", "");
  if (state.targetPlatform.empty()) {
    auto const hostPlatform = PlatformTraits::GetPlatformType();
    if (hostPlatform == PlatformType::Windows) {
      state.targetPlatform = "windows";
    } else if (hostPlatform == PlatformType::macOS) {
      state.targetPlatform = "macos";
    } else {
      state.targetPlatform = "linux";
    }
  }

  state.targetArchitecture = item.value("target_architecture", "");
  if (state.targetArchitecture.empty()) {
    state.targetArchitecture = "x64";
  }

  return state;
}
} // namespace

// -----------------------------------------------------------------------------
// SystemLogger Common Interface (Delegates debug outputs to platform files)
// -----------------------------------------------------------------------------

void SystemLogger::LogInformation(const std::string& message) {
  std::cout << "[INFO] " << message << '\n';
}

void SystemLogger::LogError(const std::string& message, const std::optional<std::string>& exceptionMessage) {
  std::cerr << "[ERROR] " << message;
  if (exceptionMessage.has_value()) {
    std::cerr << " Details: " << exceptionMessage.value();
  }
  std::cerr << '\n';
}

// -----------------------------------------------------------------------------
// ManifestManager Implementation
// -----------------------------------------------------------------------------

ManifestManager::ManifestManager(std::filesystem::path installationRootDirectory)
    : m_installationRootDirectory(std::move(installationRootDirectory)) {
  LoadManifestFromFile();
}

std::filesystem::path ManifestManager::GetManifestFilePath() const {
  return m_installationRootDirectory / "catupdate.json";
}

std::filesystem::path ManifestManager::GetInstallationRootDirectory() const {
  return m_installationRootDirectory;
}

bool ManifestManager::LoadManifestFromFile() {
  m_installedPackages.clear();
  auto filePath = GetManifestFilePath();

  if (!std::filesystem::exists(filePath)) {
    return false;
  }

  std::ifstream fileStream(filePath);
  if (!fileStream.is_open()) {
    SystemLogger::LogError("Failed to open manifest database for reading: " + filePath.string());
    return false;
  }

  try {
    nlohmann::json jsonData = nlohmann::json::parse(fileStream);

    if (jsonData.contains("packages") && jsonData["packages"].is_array()) {
      for (const auto& item : jsonData["packages"]) {
        const InstalledPackageState state = ParsePackageState(item);
        if (!state.identifier.empty() && !state.installedVersion.empty()) {
          m_installedPackages.push_back(state);
        }
      }
    }
    return true;
  } catch (const std::exception& ex) {
    SystemLogger::LogError("Exception parsing manifest JSON", ex.what());
    return false;
  }
}

bool ManifestManager::SaveManifestToFile() {
  auto filePath = GetManifestFilePath();

  try {
    std::filesystem::create_directories(m_installationRootDirectory);
  } catch (const std::exception& ex) {
    SystemLogger::LogError("Failed to create root installation directory: " + m_installationRootDirectory.string(),
                           ex.what());
    return false;
  }

  nlohmann::json jsonData;
  jsonData["install_path"] = m_installationRootDirectory.string();
  jsonData["packages"] = nlohmann::json::array();

  for (const auto& package : m_installedPackages) {
    const nlohmann::json pkgJson = {{"id", package.identifier},
                                    {"target_platform", package.targetPlatform},
                                    {"target_architecture", package.targetArchitecture},
                                    {"version", package.installedVersion},
                                    {"install_path", package.installationPath.string()},
                                    {"install_date", package.installationDate}};
    jsonData["packages"].push_back(pkgJson);
  }

  std::ofstream fileStream(filePath);
  if (!fileStream.is_open()) {
    SystemLogger::LogError("Failed to open manifest database for writing: " + filePath.string());
    return false;
  }

  fileStream << jsonData.dump(4);
  return true;
}

void ManifestManager::RegisterOrUpdateInstalledPackage(const InstalledPackageState& packageState) {
  for (auto& item : m_installedPackages) {
    if (item.identifier == packageState.identifier) {
      item = packageState;
      SaveManifestToFile();
      return;
    }
  }
  m_installedPackages.push_back(packageState);
  SaveManifestToFile();
}

void ManifestManager::UnregisterInstalledPackage(const PackageIdentifier& packageIdentifier) {
  const auto removedCount =
      std::erase_if(m_installedPackages, [&packageIdentifier](const InstalledPackageState& state) {
        return state.identifier == packageIdentifier;
      });

  if (removedCount > 0) {
    SaveManifestToFile();
  }
}

std::optional<InstalledPackageState>
ManifestManager::GetInstalledPackageByIdentifier(const PackageIdentifier& packageIdentifier) const {
  for (const auto& item : m_installedPackages) {
    if (item.identifier == packageIdentifier) {
      return item;
    }
  }
  return std::nullopt;
}

bool ManifestManager::IsPackageInstalled(const PackageIdentifier& packageIdentifier) const {
  return GetInstalledPackageByIdentifier(packageIdentifier).has_value();
}

std::vector<InstalledPackageState> ManifestManager::GetInstalledPackages() const {
  return m_installedPackages;
}
} // namespace CatUpdate
