#include "CatUpdateCore.hpp"
#include "Cli.hpp"
#include "HttpClient.hpp"
#include "ProcessExecutor.hpp"
#include "Providers.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <vector>

// ANSI Terminal Colors
constexpr const char* const COLOR_RESET = "\033[0m";
constexpr const char* const COLOR_GREEN = "\033[1;32m";
constexpr const char* const COLOR_YELLOW = "\033[1;33m";
constexpr const char* const COLOR_RED = "\033[1;31m";
constexpr const char* const COLOR_CYAN = "\033[1;36m";
constexpr const char* const COLOR_BOLD = "\033[1m";

namespace CatUpdate {

namespace {
std::string ParseVersionOverride(const std::vector<std::string>& arguments) {
  for (size_t i = 3; i < arguments.size(); ++i) {
    if (arguments[i] == "--version" && i + 1 < arguments.size()) {
      return arguments[i + 1];
    }
  }
  return "";
}
} // namespace

int CommandLineInterface::Run(const std::vector<std::string>& arguments) {
  if (arguments.size() < 2) {
    PrintUsage();
    return 0;
  }

  const std::string& command = arguments[1];

  if (command == "list") {
    return ExecuteListCommand();
  }
  if (command == "info") {
    if (arguments.size() < 3) {
      std::cerr << COLOR_RED << "Error: Missing package identifier." << COLOR_RESET << '\n';
      return 1;
    }
    return ExecuteInfoCommand(arguments[2]);
  }
  if (command == "install") {
    if (arguments.size() < 3) {
      std::cerr << COLOR_RED << "Error: Missing package identifier." << COLOR_RESET << '\n';
      return 1;
    }
    const std::string& packageId = arguments[2];
    const std::string versionOverride = ParseVersionOverride(arguments);
    return ExecuteInstallCommand(packageId, versionOverride);
  }

  if (command == "download") {
    if (arguments.size() < 3) {
      std::cerr << COLOR_RED << "Error: Missing package identifier." << COLOR_RESET << '\n';
      return 1;
    }
    const std::string& packageId = arguments[2];
    const std::string versionOverride = ParseVersionOverride(arguments);
    return ExecuteDownloadCommand(packageId, versionOverride);
  }

  if (command == "uninstall") {
    if (arguments.size() < 3) {
      std::cerr << COLOR_RED << "Error: Missing package identifier." << COLOR_RESET << '\n';
      return 1;
    }
    return ExecuteUninstallCommand(arguments[2]);
  }

  if (command == "path") {
    const std::vector<std::string> pathArgs(arguments.begin() + 2, arguments.end());
    return ExecutePathCommand(pathArgs);
  }

  std::cerr << COLOR_RED << "Error: Unknown command '" << command << "'" << COLOR_RESET << '\n';
  PrintUsage();
  return 1;
}

void CommandLineInterface::PrintUsage() {
  std::cout << COLOR_CYAN << "=== CatUpdate Software Center CLI ===" << COLOR_RESET << '\n';
  std::cout << COLOR_BOLD << "Usage:" << COLOR_RESET << '\n';
  std::cout << "  catupdate <command> [arguments]" << '\n' << '\n';
  std::cout << COLOR_BOLD << "Commands:" << COLOR_RESET << '\n';
  std::cout << "  " << COLOR_GREEN << "list" << COLOR_RESET
            << "                      List all available packages, versions, and status" << '\n';
  std::cout << "  " << COLOR_GREEN << "info <package_id>" << COLOR_RESET
            << "         Show detailed package info and versions" << '\n';
  std::cout << "  " << COLOR_GREEN << "install <package_id> [args]" << COLOR_RESET << " Install or update a package"
            << '\n';
  std::cout << "      options: --version <ver>  Install specific version" << '\n';
  std::cout << "  " << COLOR_GREEN << "download <package_id> [args]" << COLOR_RESET
            << " Download package archive to current directory" << '\n';
  std::cout << "      options: --version <ver>  Download specific version" << '\n';
  std::cout << "  " << COLOR_GREEN << "uninstall <package_id>" << COLOR_RESET << "   Remove an installed package"
            << '\n';
  std::cout << "  " << COLOR_GREEN << "path [<new_path>]" << COLOR_RESET
            << "         Display or modify the root installation directory" << '\n';
  std::cout << '\n';
}

int CommandLineInterface::ExecuteListCommand() {
  auto rootPath = PathResolver::GetDefaultInstallationRootPath();
  const ManifestManager manifest(rootPath);
  auto providers = PackageProviderRegistry::GetRegisteredProviders();

  std::cout << COLOR_BOLD << "Root Installation Directory: " << COLOR_RESET
            << manifest.GetInstallationRootDirectory().string() << '\n'
            << '\n';
  std::cout << std::format("{:<25} {:<15} {:<20} {:<15}", "Software Name", "Identifier", "Installed Version", "Status")
            << '\n';
  std::cout << std::string(78, '-') << '\n';

  for (const auto& provider : providers) {
    std::string status = "Available";
    std::string versionStr = "Not Installed";

    auto installed = manifest.GetInstalledPackageByIdentifier(provider->GetIdentifier());
    if (installed.has_value()) {
      status = std::format("{}Installed{}", COLOR_GREEN, COLOR_RESET);
      versionStr = installed->installedVersion;
    } else {
      status = std::format("{}Available{}", COLOR_YELLOW, COLOR_RESET);
      versionStr = std::format("{}None{}", COLOR_RED, COLOR_RESET);
    }

    std::cout << std::format("{:<25} {:<15} {:<30} {:<15}", provider->GetDisplayName(), provider->GetIdentifier(),
                             versionStr, status)
              << '\n';
  }

  return 0;
}

int CommandLineInterface::ExecuteInfoCommand(const std::string& packageId) {
  auto providers = PackageProviderRegistry::GetRegisteredProviders();
  auto providerIt =
      std::ranges::find_if(providers, [&](const auto& prov) { return prov->GetIdentifier() == packageId; });

  if (providerIt == providers.end()) {
    std::cerr << COLOR_RED << "Error: Package '" << packageId << "' is not available." << COLOR_RESET << '\n';
    return 1;
  }

  auto* provider = providerIt->get();
  auto httpClient = HttpClientFactory::CreateDefaultClient();

  std::cout << COLOR_CYAN << "Fetching version info for " << provider->GetDisplayName() << "..." << COLOR_RESET << '\n';
  auto versions = provider->FetchAvailableVersions(*httpClient);

  std::cout << COLOR_BOLD << "Package ID:   " << COLOR_RESET << provider->GetIdentifier() << '\n';
  std::cout << COLOR_BOLD << "Name:         " << COLOR_RESET << provider->GetDisplayName() << '\n';
  std::cout << COLOR_BOLD << "Versions:     " << COLOR_RESET;
  if (versions.empty()) {
    std::cout << COLOR_RED << "None available" << COLOR_RESET << '\n';
  } else {
    for (size_t i = 0; i < std::min(versions.size(), static_cast<size_t>(5)); ++i) {
      if (i > 0) {
        std::cout << ", ";
      }
      std::cout << versions[i];
    }
    if (versions.size() > 5) {
      std::cout << " ... (" << (versions.size() - 5) << " more)";
    }
    std::cout << '\n';
  }
  return 0;
}

int CommandLineInterface::ExecuteInstallCommand(const std::string& packageId, const std::string& versionOverride) {
  auto providers = PackageProviderRegistry::GetRegisteredProviders();
  auto providerIt =
      std::ranges::find_if(providers, [&](const auto& prov) { return prov->GetIdentifier() == packageId; });

  if (providerIt == providers.end()) {
    std::cerr << COLOR_RED << "Error: Package '" << packageId << "' is not supported or available." << COLOR_RESET
              << '\n';
    return 1;
  }

  auto* provider = providerIt->get();
  auto rootPath = PathResolver::GetDefaultInstallationRootPath();
  ManifestManager manifest(rootPath);
  auto httpClient = HttpClientFactory::CreateDefaultClient();

  std::string targetVersion = versionOverride;
  if (targetVersion.empty()) {
    std::cout << COLOR_CYAN << "Retrieving latest version for " << provider->GetDisplayName() << "..." << COLOR_RESET
              << '\n';
    auto versions = provider->FetchAvailableVersions(*httpClient);
    if (versions.empty()) {
      std::cerr << COLOR_RED << "Error: Failed to query remote version registry." << COLOR_RESET << '\n';
      return 1;
    }
    targetVersion = versions[0];
  }

  const UrlString downloadUrl = provider->GetDownloadUrl(targetVersion);
  const std::string archiveName = provider->GetArchiveFilename(targetVersion);
  const std::filesystem::path tempArchiveFile = manifest.GetInstallationRootDirectory() / ("temp_" + archiveName);
  const std::filesystem::path targetInstallationDir =
      manifest.GetInstallationRootDirectory() / provider->GetIdentifier();

  std::cout << COLOR_CYAN << std::format("Installing {} (Version: {})", provider->GetDisplayName(), targetVersion)
            << COLOR_RESET << '\n';
  std::cout << "Download URL: " << downloadUrl << '\n';

  // Ensure parent directories exist
  std::filesystem::create_directories(manifest.GetInstallationRootDirectory());

  // Download File
  const bool downloadSuccess = httpClient->DownloadFile(downloadUrl, tempArchiveFile, [](float progress) {
    std::cout << "\r[";
    const int pos = static_cast<int>(40.0F * progress);
    for (int i = 0; i < 40; ++i) {
      if (i < pos) {
        std::cout << "=";
      } else if (i == pos) {
        std::cout << ">";
      } else {
        std::cout << " ";
      }
    }
    std::cout << "] " << static_cast<int>(progress * 100.0F) << " %" << std::flush;
  });
  std::cout << '\n';

  if (!downloadSuccess) {
    std::cerr << COLOR_RED << "Error: Download failed." << COLOR_RESET << '\n';
    if (std::filesystem::exists(tempArchiveFile)) {
      std::filesystem::remove(tempArchiveFile);
    }
    return 1;
  }

  // Perform Extraction
  std::cout << COLOR_CYAN << "Extracting package files..." << COLOR_RESET << '\n';
  std::filesystem::create_directories(targetInstallationDir);

  // Construct clean, robust command line for extraction based on extension
  std::vector<std::string> extractionCommand;
#ifdef _WIN32
  // On Windows, built-in tar.exe handles zip, 7z, tar.xz etc. flawlessly.
  extractionCommand = {"tar.exe", "-xf", tempArchiveFile.string(), "-C", targetInstallationDir.string()};
#else
  if (tempArchiveFile.extension() == ".zip") {
    extractionCommand = {"unzip", "-q", "-o", tempArchiveFile.string(), "-d", targetInstallationDir.string()};
  } else {
    // tar handles gz, xz on Unix
    extractionCommand = {"tar", "-xf", tempArchiveFile.string(), "-C", targetInstallationDir.string()};
  }
#endif

  auto extractionResult = ProcessExecutor::ExecuteCommand(extractionCommand);
  std::filesystem::remove(tempArchiveFile); // Cleanup downloaded archive

  if (!extractionResult.has_value() || extractionResult->exitCode != 0) {
    std::cerr << COLOR_RED << "Error: Extraction failed." << COLOR_RESET << '\n';
    if (extractionResult.has_value()) {
      std::cerr << "Details: " << extractionResult->standardOutput << '\n';
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
  state.installationDate =
      std::format("{:04}-{:02}-{:02}", timeStruct.tm_year + 1900, timeStruct.tm_mon + 1, timeStruct.tm_mday);

  manifest.RegisterOrUpdateInstalledPackage(state);

  std::cout << COLOR_GREEN << "Installation of " << provider->GetDisplayName() << " completed successfully!"
            << COLOR_RESET << '\n';
  std::cout << "Path: " << targetInstallationDir.string() << '\n';
  return 0;
}

int CommandLineInterface::ExecuteDownloadCommand(const std::string& packageId, const std::string& versionOverride) {
  auto providers = PackageProviderRegistry::GetRegisteredProviders();
  auto providerIt =
      std::ranges::find_if(providers, [&](const auto& prov) { return prov->GetIdentifier() == packageId; });

  if (providerIt == providers.end()) {
    std::cerr << COLOR_RED << "Error: Package '" << packageId << "' is not supported or available." << COLOR_RESET
              << '\n';
    return 1;
  }

  auto* provider = providerIt->get();
  auto httpClient = HttpClientFactory::CreateDefaultClient();

  std::string targetVersion = versionOverride;
  if (targetVersion.empty()) {
    std::cout << COLOR_CYAN << "Retrieving latest version for " << provider->GetDisplayName() << "..." << COLOR_RESET
              << '\n';
    auto versions = provider->FetchAvailableVersions(*httpClient);
    if (versions.empty()) {
      std::cerr << COLOR_RED << "Error: Failed to query remote version registry." << COLOR_RESET << '\n';
      return 1;
    }
    targetVersion = versions[0];
  }

  const UrlString downloadUrl = provider->GetDownloadUrl(targetVersion);
  const std::string archiveName = provider->GetArchiveFilename(targetVersion);
  const std::filesystem::path targetFile = std::filesystem::current_path() / archiveName;

  std::cout << COLOR_CYAN
            << std::format("Downloading {} (Version: {}) to current directory", provider->GetDisplayName(),
                           targetVersion)
            << COLOR_RESET << '\n';
  std::cout << "Destination:  " << targetFile.string() << '\n';
  std::cout << "Download URL: " << downloadUrl << '\n';

  // Download File
  const bool downloadSuccess = httpClient->DownloadFile(downloadUrl, targetFile, [](float progress) {
    std::cout << "\r[";
    const int pos = static_cast<int>(40.0F * progress);
    for (int i = 0; i < 40; ++i) {
      if (i < pos) {
        std::cout << "=";
      } else if (i == pos) {
        std::cout << ">";
      } else {
        std::cout << " ";
      }
    }
    std::cout << "] " << static_cast<int>(progress * 100.0F) << " %" << std::flush;
  });
  std::cout << '\n';

  if (!downloadSuccess) {
    std::cerr << COLOR_RED << "Error: Download failed." << COLOR_RESET << '\n';
    if (std::filesystem::exists(targetFile)) {
      std::filesystem::remove(targetFile);
    }
    return 1;
  }

  std::cout << COLOR_GREEN << "Download completed successfully!" << COLOR_RESET << '\n';
  return 0;
}

int CommandLineInterface::ExecuteUninstallCommand(const std::string& packageId) {
  auto rootPath = PathResolver::GetDefaultInstallationRootPath();
  ManifestManager manifest(rootPath);

  auto packageState = manifest.GetInstalledPackageByIdentifier(packageId);
  if (!packageState.has_value()) {
    std::cerr << COLOR_RED << "Error: Package '" << packageId << "' is not registered as installed." << COLOR_RESET
              << '\n';
    return 1;
  }

  std::cout << COLOR_CYAN << "Uninstalling package at " << packageState->installationPath.string() << "..."
            << COLOR_RESET << '\n';

  try {
    if (std::filesystem::exists(packageState->installationPath)) {
      std::filesystem::remove_all(packageState->installationPath);
    }

    manifest.UnregisterInstalledPackage(packageId);

    std::cout << COLOR_GREEN << "Uninstall completed successfully!" << COLOR_RESET << '\n';
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << COLOR_RED << "Error during uninstall: " << ex.what() << COLOR_RESET << '\n';
    return 1;
  }
}

int CommandLineInterface::ExecutePathCommand(const std::vector<std::string>& pathArguments) {
  auto rootPath = PathResolver::GetDefaultInstallationRootPath();
  const ManifestManager manifest(rootPath);

  if (pathArguments.empty()) {
    std::cout << "Current Root Installation Directory: " << manifest.GetInstallationRootDirectory().string() << '\n';
    return 0;
  }

  const std::filesystem::path newPath(pathArguments[0]);
  std::cout << COLOR_CYAN << "Changing root installation directory to: " << newPath.string() << COLOR_RESET << '\n';

  try {
    std::filesystem::create_directories(newPath);
    // Move old manifest if it exists to keep package state? Or start a new manifest.
    // We'll start a new manifest at that path.
    ManifestManager newManifest(newPath);
    newManifest.SaveManifestToFile();

    std::cout << COLOR_GREEN << "Path updated successfully!" << COLOR_RESET << '\n';
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << COLOR_RED << "Failed to change path: " << ex.what() << COLOR_RESET << '\n';
    return 1;
  }
}

} // namespace CatUpdate

#if defined(_WIN32) && defined(UNICODE)
int wmain(int argc, wchar_t* argv[]) noexcept {
  try {
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
      args.push_back(CatUpdate::Utils::ToString(argv[i]));
    }
    return CatUpdate::CommandLineInterface::Run(args);
  } catch (const std::exception& ex) {
    std::cerr << "Unhandled exception: " << ex.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Unknown unhandled exception occurred\n";
    return 1;
  }
}
#else
int main(int argc, char* argv[]) noexcept {
  try {
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
      args.emplace_back(argv[i]);
    }
    return CatUpdate::CommandLineInterface::Run(args);
  } catch (const std::exception& ex) {
    std::cerr << "Unhandled exception: " << ex.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Unknown unhandled exception occurred\n";
    return 1;
  }
}
#endif
