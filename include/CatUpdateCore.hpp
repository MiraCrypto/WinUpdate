#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <functional>

namespace CatUpdate {

/**
 * Domain-specific strongly typed definitions to avoid primitive string confusion.
 */
using PackageIdentifier = std::string;
using PackageName = std::string;
using PackageVersion = std::string;
using UrlString = std::string;

/**
 * Holds the core properties of a downloadable software package.
 */
struct PackageMetadata {
    PackageIdentifier identifier;
    PackageName name;
    PackageVersion version;
    UrlString downloadUrl;
    std::string archiveFilename;
};

/**
 * Holds the state of a package currently installed on the local machine.
 */
struct InstalledPackageState {
    PackageIdentifier identifier;
    PackageVersion installedVersion;
    std::filesystem::path installationPath;
    std::string installationDate;
};

/**
 * Utilities for system-level logging and diagnostics.
 */
class SystemLogger {
public:
    static void LogInformation(const std::string& message);
    static void LogError(const std::string& message, const std::optional<std::string>& exceptionMessage = std::nullopt);
};

/**
 * Common string and system utilities.
 */
class Utils {
public:
    /**
     * Converts a UTF-8 string to a UTF-16 wstring.
     */
    static std::wstring ToWString(const std::string& utf8Str);

    /**
     * Converts a UTF-16 wstring to a UTF-8 string.
     */
    static std::string ToString(const std::wstring& utf16Str);
};

/**
 * Handles platform-aware path resolutions for default installation targets.
 */
class PathResolver {
public:
    /**
     * Resolves the default installation root directory.
     * - Windows: C:\Users\Public\Documents (without any subdirectories)
     * - Linux / macOS: ~/.local
     */
    static std::filesystem::path GetDefaultInstallationRootPath();

    /**
     * Resolves the absolute home directory of the current user.
     */
    static std::filesystem::path GetUserHomeDirectory();
};

/**
 * Manages persistence of the installed package database (catupdate.json).
 */
class ManifestManager {
public:
    explicit ManifestManager(const std::filesystem::path& installationRootDirectory);

    /**
     * Synchronizes the in-memory state with the physical JSON manifest file.
     */
    bool LoadManifestFromFile();
    bool SaveManifestToFile();

    /**
     * Records or updates an installed package status inside the database.
     */
    void RegisterOrUpdateInstalledPackage(const InstalledPackageState& packageState);

    /**
     * Queries the database for an installed package matching the identifier.
     */
    std::optional<InstalledPackageState> GetInstalledPackageByIdentifier(const PackageIdentifier& packageIdentifier) const;

    /**
     * Checks if a specific package identifier is currently registered in the system.
     */
    bool IsPackageInstalled(const PackageIdentifier& packageIdentifier) const;

    /**
     * Returns all currently registered packages.
     */
    std::vector<InstalledPackageState> GetInstalledPackages() const;

    /**
     * Returns the path where the manifest file itself is stored.
     */
    std::filesystem::path GetManifestFilePath() const;

    /**
     * Returns the parent installation root directory.
     */
    std::filesystem::path GetInstallationRootDirectory() const;

private:
    std::filesystem::path m_installationRootDirectory;
    std::vector<InstalledPackageState> m_installedPackages;
};

} // namespace CatUpdate
