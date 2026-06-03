#include "CatUpdateCore.hpp"
#include "json.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <format>
#include <chrono>

namespace CatUpdate {

// -----------------------------------------------------------------------------
// SystemLogger Common Interface (Delegates debug outputs to platform files)
// -----------------------------------------------------------------------------

void SystemLogger::LogInformation(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

void SystemLogger::LogError(const std::string& message, const std::optional<std::string>& exceptionMessage) {
    std::cerr << "[ERROR] " << message;
    if (exceptionMessage.has_value()) {
        std::cerr << " Details: " << exceptionMessage.value();
    }
    std::cerr << std::endl;
}

// -----------------------------------------------------------------------------
// ManifestManager Implementation
// -----------------------------------------------------------------------------

ManifestManager::ManifestManager(const std::filesystem::path& installationRootDirectory)
    : m_installationRootDirectory(installationRootDirectory) {
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
                InstalledPackageState state;
                state.identifier = item.value("id", "");
                state.installedVersion = item.value("version", "");
                state.installationPath = std::filesystem::path(item.value("install_path", ""));
                state.installationDate = item.value("install_date", "");
                
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
        SystemLogger::LogError("Failed to create root installation directory: " + m_installationRootDirectory.string(), ex.what());
        return false;
    }

    nlohmann::json jsonData;
    jsonData["install_path"] = m_installationRootDirectory.string();
    jsonData["packages"] = nlohmann::json::array();

    for (const auto& package : m_installedPackages) {
        nlohmann::json pkgJson = {
            {"id", package.identifier},
            {"version", package.installedVersion},
            {"install_path", package.installationPath.string()},
            {"install_date", package.installationDate}
        };
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
    auto iterator = std::remove_if(m_installedPackages.begin(), m_installedPackages.end(),
        [&packageIdentifier](const InstalledPackageState& state) {
            return state.identifier == packageIdentifier;
        });

    if (iterator != m_installedPackages.end()) {
        m_installedPackages.erase(iterator, m_installedPackages.end());
        SaveManifestToFile();
    }
}

std::optional<InstalledPackageState> ManifestManager::GetInstalledPackageByIdentifier(
    const PackageIdentifier& packageIdentifier
) const {
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
