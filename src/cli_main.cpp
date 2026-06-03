#include "Cli.hpp"
#include "CatUpdateCore.hpp"
#include "HttpClient.hpp"
#include "Providers.hpp"
#include "ProcessExecutor.hpp"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <format>
#include <algorithm>
#include <chrono>

// ANSI Terminal Colors
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_BOLD    "\033[1m"

namespace CatUpdate {

int CommandLineInterface::Run(const std::vector<std::string>& arguments) {
    if (arguments.size() < 2) {
        PrintUsage();
        return 0;
    }

    std::string command = arguments[1];
    
    if (command == "list") {
        return ExecuteListCommand();
    } else if (command == "info") {
        if (arguments.size() < 3) {
            std::cerr << COLOR_RED << "Error: Missing package identifier." << COLOR_RESET << std::endl;
            return 1;
        }
        return ExecuteInfoCommand(arguments[2]);
    } else if (command == "install") {
        if (arguments.size() < 3) {
            std::cerr << COLOR_RED << "Error: Missing package identifier." << COLOR_RESET << std::endl;
            return 1;
        }
        std::string packageId = arguments[2];
        std::string versionOverride;
        for (size_t i = 3; i < arguments.size(); ++i) {
            if (arguments[i] == "--version" && i + 1 < arguments.size()) {
                versionOverride = arguments[i + 1];
                break;
            }
        }
        return ExecuteInstallCommand(packageId, versionOverride);
    } else if (command == "download") {
        if (arguments.size() < 3) {
            std::cerr << COLOR_RED << "Error: Missing package identifier." << COLOR_RESET << std::endl;
            return 1;
        }
        std::string packageId = arguments[2];
        std::string versionOverride;
        for (size_t i = 3; i < arguments.size(); ++i) {
            if (arguments[i] == "--version" && i + 1 < arguments.size()) {
                versionOverride = arguments[i + 1];
                break;
            }
        }
        return ExecuteDownloadCommand(packageId, versionOverride);
    } else if (command == "uninstall") {
        if (arguments.size() < 3) {
            std::cerr << COLOR_RED << "Error: Missing package identifier." << COLOR_RESET << std::endl;
            return 1;
        }
        return ExecuteUninstallCommand(arguments[2]);
    } else if (command == "path") {
        std::vector<std::string> pathArgs(arguments.begin() + 2, arguments.end());
        return ExecutePathCommand(pathArgs);
    } else {
        std::cerr << COLOR_RED << "Error: Unknown command '" << command << "'" << COLOR_RESET << std::endl;
        PrintUsage();
        return 1;
    }
}

void CommandLineInterface::PrintUsage() {
    std::cout << COLOR_CYAN << "=== CatUpdate Software Center CLI ===" << COLOR_RESET << std::endl;
    std::cout << COLOR_BOLD << "Usage:" << COLOR_RESET << std::endl;
    std::cout << "  catupdate <command> [arguments]" << std::endl << std::endl;
    std::cout << COLOR_BOLD << "Commands:" << COLOR_RESET << std::endl;
    std::cout << "  " << COLOR_GREEN << "list" << COLOR_RESET << "                      List all available packages, versions, and status" << std::endl;
    std::cout << "  " << COLOR_GREEN << "info <package_id>" << COLOR_RESET << "         Show detailed package info and versions" << std::endl;
    std::cout << "  " << COLOR_GREEN << "install <package_id> [args]" << COLOR_RESET << " Install or update a package" << std::endl;
    std::cout << "      options: --version <ver>  Install specific version" << std::endl;
    std::cout << "  " << COLOR_GREEN << "download <package_id> [args]" << COLOR_RESET << " Download package archive to current directory" << std::endl;
    std::cout << "      options: --version <ver>  Download specific version" << std::endl;
    std::cout << "  " << COLOR_GREEN << "uninstall <package_id>" << COLOR_RESET << "   Remove an installed package" << std::endl;
    std::cout << "  " << COLOR_GREEN << "path [<new_path>]" << COLOR_RESET << "         Display or modify the root installation directory" << std::endl;
    std::cout << std::endl;
}

int CommandLineInterface::ExecuteListCommand() {
    auto rootPath = PathResolver::GetDefaultInstallationRootPath();
    ManifestManager manifest(rootPath);
    auto providers = PackageProviderRegistry::GetRegisteredProviders();

    std::cout << COLOR_BOLD << "Root Installation Directory: " << COLOR_RESET << manifest.GetInstallationRootDirectory().string() << std::endl << std::endl;
    std::cout << std::format("{:<25} {:<15} {:<20} {:<15}", "Software Name", "Identifier", "Installed Version", "Status") << std::endl;
    std::cout << std::string(78, '-') << std::endl;

    for (const auto& provider : providers) {
        std::string status = "Available";
        std::string versionStr = "Not Installed";
        
        auto installed = manifest.GetInstalledPackageByIdentifier(provider->GetIdentifier());
        if (installed.has_value()) {
            status = COLOR_GREEN "Installed" COLOR_RESET;
            versionStr = installed->installedVersion;
        } else {
            status = COLOR_YELLOW "Available" COLOR_RESET;
            versionStr = COLOR_RED "None" COLOR_RESET;
        }

        std::cout << std::format("{:<25} {:<15} {:<30} {:<15}", 
            provider->GetDisplayName(), 
            provider->GetIdentifier(), 
            versionStr, 
            status
        ) << std::endl;
    }

    return 0;
}

int CommandLineInterface::ExecuteInfoCommand(const std::string& packageId) {
    auto providers = PackageProviderRegistry::GetRegisteredProviders();
    auto providerIt = std::find_if(providers.begin(), providers.end(), [&](const auto& p) {
        return p->GetIdentifier() == packageId;
    });

    if (providerIt == providers.end()) {
        std::cerr << COLOR_RED << "Error: Package '" << packageId << "' is not available." << COLOR_RESET << std::endl;
        return 1;
    }

    auto provider = providerIt->get();
    auto httpClient = HttpClientFactory::CreateDefaultClient();

    std::cout << COLOR_CYAN << "Fetching version info for " << provider->GetDisplayName() << "..." << COLOR_RESET << std::endl;
    auto versions = provider->FetchAvailableVersions(*httpClient);

    std::cout << COLOR_BOLD << "Package ID:   " << COLOR_RESET << provider->GetIdentifier() << std::endl;
    std::cout << COLOR_BOLD << "Name:         " << COLOR_RESET << provider->GetDisplayName() << std::endl;
    std::cout << COLOR_BOLD << "Versions:     " << COLOR_RESET;
    if (versions.empty()) {
        std::cout << COLOR_RED << "None available" << COLOR_RESET << std::endl;
    } else {
        for (size_t i = 0; i < std::min(versions.size(), size_t(5)); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << versions[i];
        }
        if (versions.size() > 5) {
            std::cout << " ... (" << (versions.size() - 5) << " more)";
        }
        std::cout << std::endl;
    }
    return 0;
}

int CommandLineInterface::ExecuteInstallCommand(const std::string& packageId, const std::string& versionOverride) {
    auto providers = PackageProviderRegistry::GetRegisteredProviders();
    auto providerIt = std::find_if(providers.begin(), providers.end(), [&](const auto& p) {
        return p->GetIdentifier() == packageId;
    });

    if (providerIt == providers.end()) {
        std::cerr << COLOR_RED << "Error: Package '" << packageId << "' is not supported or available." << COLOR_RESET << std::endl;
        return 1;
    }

    auto provider = providerIt->get();
    auto rootPath = PathResolver::GetDefaultInstallationRootPath();
    ManifestManager manifest(rootPath);
    auto httpClient = HttpClientFactory::CreateDefaultClient();

    std::string targetVersion = versionOverride;
    if (targetVersion.empty()) {
        std::cout << COLOR_CYAN << "Retrieving latest version for " << provider->GetDisplayName() << "..." << COLOR_RESET << std::endl;
        auto versions = provider->FetchAvailableVersions(*httpClient);
        if (versions.empty()) {
            std::cerr << COLOR_RED << "Error: Failed to query remote version registry." << COLOR_RESET << std::endl;
            return 1;
        }
        targetVersion = versions[0];
    }

    UrlString downloadUrl = provider->GetDownloadUrl(targetVersion);
    std::string archiveName = provider->GetArchiveFilename(targetVersion);
    std::filesystem::path tempArchiveFile = manifest.GetInstallationRootDirectory() / ("temp_" + archiveName);
    std::filesystem::path targetInstallationDir = manifest.GetInstallationRootDirectory() / provider->GetIdentifier();

    std::cout << COLOR_CYAN << std::format("Installing {} (Version: {})", provider->GetDisplayName(), targetVersion) << COLOR_RESET << std::endl;
    std::cout << "Download URL: " << downloadUrl << std::endl;

    // Ensure parent directories exist
    std::filesystem::create_directories(manifest.GetInstallationRootDirectory());

    // Download File
    bool downloadSuccess = httpClient->DownloadFile(downloadUrl, tempArchiveFile, [](float progress) {
        int barWidth = 40;
        std::cout << "\r[";
        int pos = static_cast<int>(barWidth * progress);
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << static_cast<int>(progress * 100.0) << " %" << std::flush;
    });
    std::cout << std::endl;

    if (!downloadSuccess) {
        std::cerr << COLOR_RED << "Error: Download failed." << COLOR_RESET << std::endl;
        if (std::filesystem::exists(tempArchiveFile)) {
            std::filesystem::remove(tempArchiveFile);
        }
        return 1;
    }

    // Perform Extraction
    std::cout << COLOR_CYAN << "Extracting package files..." << COLOR_RESET << std::endl;
    std::filesystem::create_directories(targetInstallationDir);

    // Construct clean, robust command line for extraction based on extension
    std::vector<std::string> extractionCommand;
#if defined(_WIN32)
    // On Windows, built-in tar.exe handles zip, 7z, tar.xz etc. flawlessly.
    extractionCommand = {
        "tar.exe",
        "-xf",
        tempArchiveFile.string(),
        "-C",
        targetInstallationDir.string()
    };
#else
    if (tempArchiveFile.extension() == ".zip") {
        extractionCommand = {
            "unzip",
            "-q",
            "-o",
            tempArchiveFile.string(),
            "-d",
            targetInstallationDir.string()
        };
    } else {
        // tar handles gz, xz on Unix
        extractionCommand = {
            "tar",
            "-xf",
            tempArchiveFile.string(),
            "-C",
            targetInstallationDir.string()
        };
    }
#endif

    auto extractionResult = ProcessExecutor::ExecuteCommand(extractionCommand);
    std::filesystem::remove(tempArchiveFile); // Cleanup downloaded archive

    if (!extractionResult.has_value() || extractionResult->exitCode != 0) {
        std::cerr << COLOR_RED << "Error: Extraction failed." << COLOR_RESET << std::endl;
        if (extractionResult.has_value()) {
            std::cerr << "Details: " << extractionResult->standardOutput << std::endl;
        }
        return 1;
    }

    // Track in manifest
    InstalledPackageState state;
    state.identifier = provider->GetIdentifier();
    state.installedVersion = targetVersion;
    state.installationPath = targetInstallationDir;
    
    // Get current date
    auto now = std::chrono::system_clock::now();
    std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm timeStruct = *std::localtime(&nowTime);
    state.installationDate = std::format("{:04}-{:02}-{:02}", 
        timeStruct.tm_year + 1900, timeStruct.tm_mon + 1, timeStruct.tm_mday);

    manifest.RegisterOrUpdateInstalledPackage(state);

    std::cout << COLOR_GREEN << "Installation of " << provider->GetDisplayName() << " completed successfully!" << COLOR_RESET << std::endl;
    std::cout << "Path: " << targetInstallationDir.string() << std::endl;
    return 0;
}

int CommandLineInterface::ExecuteDownloadCommand(const std::string& packageId, const std::string& versionOverride) {
    auto providers = PackageProviderRegistry::GetRegisteredProviders();
    auto providerIt = std::find_if(providers.begin(), providers.end(), [&](const auto& p) {
        return p->GetIdentifier() == packageId;
    });

    if (providerIt == providers.end()) {
        std::cerr << COLOR_RED << "Error: Package '" << packageId << "' is not supported or available." << COLOR_RESET << std::endl;
        return 1;
    }

    auto provider = providerIt->get();
    auto httpClient = HttpClientFactory::CreateDefaultClient();

    std::string targetVersion = versionOverride;
    if (targetVersion.empty()) {
        std::cout << COLOR_CYAN << "Retrieving latest version for " << provider->GetDisplayName() << "..." << COLOR_RESET << std::endl;
        auto versions = provider->FetchAvailableVersions(*httpClient);
        if (versions.empty()) {
            std::cerr << COLOR_RED << "Error: Failed to query remote version registry." << COLOR_RESET << std::endl;
            return 1;
        }
        targetVersion = versions[0];
    }

    UrlString downloadUrl = provider->GetDownloadUrl(targetVersion);
    std::string archiveName = provider->GetArchiveFilename(targetVersion);
    std::filesystem::path targetFile = std::filesystem::current_path() / archiveName;

    std::cout << COLOR_CYAN << std::format("Downloading {} (Version: {}) to current directory", provider->GetDisplayName(), targetVersion) << COLOR_RESET << std::endl;
    std::cout << "Destination:  " << targetFile.string() << std::endl;
    std::cout << "Download URL: " << downloadUrl << std::endl;

    // Download File
    bool downloadSuccess = httpClient->DownloadFile(downloadUrl, targetFile, [](float progress) {
        int barWidth = 40;
        std::cout << "\r[";
        int pos = static_cast<int>(barWidth * progress);
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << static_cast<int>(progress * 100.0) << " %" << std::flush;
    });
    std::cout << std::endl;

    if (!downloadSuccess) {
        std::cerr << COLOR_RED << "Error: Download failed." << COLOR_RESET << std::endl;
        if (std::filesystem::exists(targetFile)) {
            std::filesystem::remove(targetFile);
        }
        return 1;
    }

    std::cout << COLOR_GREEN << "Download completed successfully!" << COLOR_RESET << std::endl;
    return 0;
}

int CommandLineInterface::ExecuteUninstallCommand(const std::string& packageId) {
    auto rootPath = PathResolver::GetDefaultInstallationRootPath();
    ManifestManager manifest(rootPath);

    auto packageState = manifest.GetInstalledPackageByIdentifier(packageId);
    if (!packageState.has_value()) {
        std::cerr << COLOR_RED << "Error: Package '" << packageId << "' is not registered as installed." << COLOR_RESET << std::endl;
        return 1;
    }

    std::cout << COLOR_CYAN << "Uninstalling package at " << packageState->installationPath.string() << "..." << COLOR_RESET << std::endl;

    try {
        if (std::filesystem::exists(packageState->installationPath)) {
            std::filesystem::remove_all(packageState->installationPath);
        }
        
        // Remove from json
        std::vector<InstalledPackageState> remainingPackages;
        for (const auto& pkg : manifest.GetInstalledPackages()) {
            if (pkg.identifier != packageId) {
                remainingPackages.push_back(pkg);
            }
        }

        // We need to save the remaining. Let's clear manifest and update.
        // To support this, we can re-create manifest or rewrite it.
        // ManifestManager doesn't have a Direct Delete, let's register them or overwrite them.
        // Let's create a temporary manifest and copy packages, then save.
        // Actually, we can overwrite the file or add a method. Let's write the manifest.
        std::filesystem::path manifestPath = manifest.GetManifestFilePath();
        nlohmann::json jsonData;
        jsonData["install_path"] = manifest.GetInstallationRootDirectory().string();
        jsonData["packages"] = nlohmann::json::array();
        for (const auto& pkg : remainingPackages) {
            jsonData["packages"].push_back({
                {"id", pkg.identifier},
                {"version", pkg.installedVersion},
                {"install_path", pkg.installationPath.string()},
                {"install_date", pkg.installationDate}
            });
        }
        std::ofstream file(manifestPath);
        file << jsonData.dump(4);

        std::cout << COLOR_GREEN << "Uninstall completed successfully!" << COLOR_RESET << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << COLOR_RED << "Error during uninstall: " << ex.what() << COLOR_RESET << std::endl;
        return 1;
    }
}

int CommandLineInterface::ExecutePathCommand(const std::vector<std::string>& pathArguments) {
    auto rootPath = PathResolver::GetDefaultInstallationRootPath();
    ManifestManager manifest(rootPath);

    if (pathArguments.empty()) {
        std::cout << "Current Root Installation Directory: " << manifest.GetInstallationRootDirectory().string() << std::endl;
        return 0;
    }

    std::filesystem::path newPath(pathArguments[0]);
    std::cout << COLOR_CYAN << "Changing root installation directory to: " << newPath.string() << COLOR_RESET << std::endl;

    try {
        std::filesystem::create_directories(newPath);
        // Move old manifest if it exists to keep package state? Or start a new manifest.
        // We'll start a new manifest at that path.
        ManifestManager newManifest(newPath);
        newManifest.SaveManifestToFile();
        
        std::cout << COLOR_GREEN << "Path updated successfully!" << COLOR_RESET << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << COLOR_RED << "Failed to change path: " << ex.what() << COLOR_RESET << std::endl;
        return 1;
    }
}

} // namespace CatUpdate

#if defined(_WIN32) && defined(UNICODE)
int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(CatUpdate::Utils::ToString(argv[i]));
    }
    return CatUpdate::CommandLineInterface::Run(args);
}
#else
int main(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return CatUpdate::CommandLineInterface::Run(args);
}
#endif
