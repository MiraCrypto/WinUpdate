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
    state.targetPlatform = PlatformTraits::PlatformToString(PlatformTraits::GetPlatformType());
  }

  state.targetArchitecture = item.value("target_architecture", "");
  if (state.targetArchitecture.empty()) {
    state.targetArchitecture = "x64";
  }

  return state;
}
std::string CleanVersion(const std::string& versionString) {
  size_t const firstDigit = versionString.find_first_of("0123456789");
  if (firstDigit == std::string::npos) {
    return "";
  }
  std::string result;
  for (size_t i = firstDigit; i < versionString.size(); ++i) {
    char const character = versionString[i];
    if (character == '+') {
      break;
    }
    if ((std::isdigit(character) != 0) || character == '.' || character == '-' || character == '_') {
      result += character;
    } else {
      break;
    }
  }
  return result;
}

std::vector<std::string> SplitVersion(const std::string& sourceString) {
  std::vector<std::string> parts;
  std::string part;
  for (char const character : sourceString) {
    if (character == '.' || character == '-' || character == '+') {
      if (!part.empty()) {
        parts.push_back(part);
        part.clear();
      }
    } else {
      part += character;
    }
  }
  if (!part.empty()) {
    parts.push_back(part);
  }
  return parts;
}

bool IsAllDigits(const std::string& str) {
  if (str.empty()) {
    return false;
  }
  return std::ranges::all_of(str, [](char const character) { return std::isdigit(character) != 0; });
}

int CompareComponents(const std::string& component1, const std::string& component2) {
  bool const isNum1 = IsAllDigits(component1);
  bool const isNum2 = IsAllDigits(component2);

  if (isNum1 && isNum2) {
    try {
      int const val1 = std::stoi(component1);
      int const val2 = std::stoi(component2);
      if (val1 < val2) {
        return -1;
      }
      if (val1 > val2) {
        return 1;
      }
      return 0;
    } catch (const std::exception& ex) {
      SystemLogger::LogError("CompareComponents parsing error", ex.what());
    }
  }

  int const comp = component1.compare(component2);
  if (comp < 0) {
    return -1;
  }
  if (comp > 0) {
    return 1;
  }
  return 0;
}
} // namespace

int Utils::CompareVersions(const std::string& versionString1, const std::string& versionString2) {
  auto const parts1 = SplitVersion(CleanVersion(versionString1));
  auto const parts2 = SplitVersion(CleanVersion(versionString2));

  size_t const maxLen = std::max(parts1.size(), parts2.size());
  for (size_t i = 0; i < maxLen; ++i) {
    std::string const component1 = (i < parts1.size()) ? parts1[i] : "0";
    std::string const component2 = (i < parts2.size()) ? parts2[i] : "0";

    int const comp = CompareComponents(component1, component2);
    if (comp != 0) {
      return comp;
    }
  }
  return 0;
}

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
