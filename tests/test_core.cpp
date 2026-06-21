#include "CatUpdateCore.hpp"
#include "PackageManager.hpp"
#include "PlatformTraits.hpp"
#include "Providers.hpp"
#include "test_helper.hpp"
#include <filesystem>
#include <string>

namespace CatUpdate {

TEST(PathResolverTest, HomeDirectoryResolution) {
  auto homeDir = PathResolver::GetUserHomeDirectory();
  ASSERT_FALSE(homeDir.empty());
  ASSERT_TRUE(std::filesystem::exists(homeDir) || homeDir == "/");
}

TEST(PathResolverTest, DefaultInstallPathResolution) {
  auto rootPath = PathResolver::GetDefaultInstallationRootPath();
  ASSERT_FALSE(rootPath.empty());
#ifdef _WIN32
  // Public Documents typically contains "Public" or "Documents"
  std::string const pathStr = rootPath.string();
  ASSERT_TRUE(pathStr.find("Public") != std::string::npos || pathStr.find("Documents") != std::string::npos);
#else
  // ~/.local
  ASSERT_TRUE(rootPath.filename() == ".local");
#endif
}

TEST(ManifestManagerTest, SaveAndLoadCycle) {
  // Create localized temporary sandbox inside workspace for deterministic testing
  const std::filesystem::path sandboxDir = std::filesystem::current_path() / "test_sandbox";
  std::filesystem::create_directories(sandboxDir);

  // Cleanup sandbox from previous runs if any
  const std::filesystem::path manifestFilePath = sandboxDir / "catupdate.json";
  if (std::filesystem::exists(manifestFilePath)) {
    std::filesystem::remove(manifestFilePath);
  }

  {
    ManifestManager manager(sandboxDir);
    ASSERT_FALSE(manager.IsPackageInstalled("nodejs"));

    InstalledPackageState pkg;
    pkg.identifier = "nodejs";
    pkg.installedVersion = "20.11.1";
    pkg.installationPath = sandboxDir / "nodejs";
    pkg.installationDate = "2026-06-01";

    manager.RegisterOrUpdateInstalledPackage(pkg);
    ASSERT_TRUE(manager.IsPackageInstalled("nodejs"));

    auto query = manager.GetInstalledPackageByIdentifier("nodejs");
    ASSERT_TRUE(query.has_value());
    ASSERT_TRUE(query->installedVersion == "20.11.1");
    ASSERT_TRUE(query->installationDate == "2026-06-01");
  }

  // Test restoration in a new ManifestManager instance (verifying serialization)
  {
    const ManifestManager manager(sandboxDir);
    ASSERT_TRUE(manager.IsPackageInstalled("nodejs"));

    auto query = manager.GetInstalledPackageByIdentifier("nodejs");
    ASSERT_TRUE(query.has_value());
    ASSERT_TRUE(query->installedVersion == "20.11.1");
    ASSERT_TRUE(query->installationDate == "2026-06-01");
  }

  // Test unregistration and serialization
  {
    ManifestManager manager(sandboxDir);
    manager.UnregisterInstalledPackage("nodejs");
    ASSERT_FALSE(manager.IsPackageInstalled("nodejs"));
  }

  // Verify file persistence of unregistration
  {
    const ManifestManager manager(sandboxDir);
    ASSERT_FALSE(manager.IsPackageInstalled("nodejs"));
  }

  // Clean up sandbox
  std::filesystem::remove_all(sandboxDir);
}

TEST(UtilsTest, WideStringConversions) {
  const std::string originalUtf8 = "Hello, CatUpdate package manager! 🐱";
  const std::wstring convertedWString = Utils::ToWString(originalUtf8);
  ASSERT_FALSE(convertedWString.empty());

  const std::string roundTripUtf8 = Utils::ToString(convertedWString);
  ASSERT_TRUE(originalUtf8 == roundTripUtf8);
}

TEST(PlatformTraitsTest, GetHostArchitecture) {
  auto const arch = PlatformTraits::GetHostArchitecture();
  ASSERT_TRUE(arch == PlatformTraits::GetHostArchitecture());
}

TEST(PlatformTraitsTest, GetPlatformPropertiesStatic) {
  // Test Windows properties
  ASSERT_TRUE(PlatformTraits::GetPlatformNameString(PlatformType::Windows, ArchitectureType::X64) == "win-x64");
  ASSERT_TRUE(PlatformTraits::GetPlatformNameString(PlatformType::Windows, ArchitectureType::Arm64) == "win-arm64");
  ASSERT_TRUE(PlatformTraits::GetArchiveExtension(PlatformType::Windows, ArchitectureType::X64) == ".zip");

  // Test macOS properties
  ASSERT_TRUE(PlatformTraits::GetPlatformNameString(PlatformType::macOS, ArchitectureType::X64) == "darwin-x64");
  ASSERT_TRUE(PlatformTraits::GetPlatformNameString(PlatformType::macOS, ArchitectureType::Arm64) == "darwin-arm64");
  ASSERT_TRUE(PlatformTraits::GetArchiveExtension(PlatformType::macOS, ArchitectureType::X64) == ".tar.gz");

  // Test Linux properties
  ASSERT_TRUE(PlatformTraits::GetPlatformNameString(PlatformType::Linux, ArchitectureType::X64) == "linux-x64");
  ASSERT_TRUE(PlatformTraits::GetPlatformNameString(PlatformType::Linux, ArchitectureType::Arm64) == "linux-arm64");
  ASSERT_TRUE(PlatformTraits::GetArchiveExtension(PlatformType::Linux, ArchitectureType::X64) == ".tar.xz");
}

TEST(PlatformTraitsTest, GetWindowsExtractionCommand) {
  const std::filesystem::path archive = "test_archive.zip";
  const std::filesystem::path dest = "test_destination";

  auto cmd = PlatformTraits::GetExtractionCommand(PlatformType::Windows, archive, dest);
  ASSERT_FALSE(cmd.empty());
  ASSERT_TRUE(cmd.size() == 5);
  ASSERT_TRUE(cmd[0] == "tar.exe");
  ASSERT_TRUE(cmd[1] == "-xf");
  ASSERT_TRUE(cmd[2] == archive.string());
  ASSERT_TRUE(cmd[3] == "-C");
  ASSERT_TRUE(cmd[4] == dest.string());
}

TEST(PlatformTraitsTest, GetPosixZipExtractionCommand) {
  const std::filesystem::path archive = "test_archive.zip";
  const std::filesystem::path dest = "test_destination";

  auto cmd = PlatformTraits::GetExtractionCommand(PlatformType::macOS, archive, dest);
  ASSERT_FALSE(cmd.empty());
  ASSERT_TRUE(cmd.size() == 6);
  ASSERT_TRUE(cmd[0] == "unzip");
  ASSERT_TRUE(cmd[1] == "-q");
  ASSERT_TRUE(cmd[2] == "-o");
  ASSERT_TRUE(cmd[3] == archive.string());
  ASSERT_TRUE(cmd[4] == "-d");
  ASSERT_TRUE(cmd[5] == dest.string());
}

TEST(PlatformTraitsTest, GetPosixTarExtractionCommand) {
  const std::filesystem::path archive = "test_archive.tar.gz";
  const std::filesystem::path dest = "test_destination";

  auto cmd = PlatformTraits::GetExtractionCommand(PlatformType::macOS, archive, dest);
  ASSERT_FALSE(cmd.empty());
  ASSERT_TRUE(cmd.size() == 5);
  ASSERT_TRUE(cmd[0] == "tar");
  ASSERT_TRUE(cmd[1] == "-xf");
  ASSERT_TRUE(cmd[2] == archive.string());
  ASSERT_TRUE(cmd[3] == "-C");
  ASSERT_TRUE(cmd[4] == dest.string());
}

TEST(PlatformTraitsTest, ActivePlatformPropertiesMatch) {
  auto type = PlatformTraits::GetPlatformType();
  auto arch = PlatformTraits::GetHostArchitecture();
  auto name = PlatformTraits::GetPlatformNameString();
  auto ext = PlatformTraits::GetArchiveExtension();

  ASSERT_TRUE(name == PlatformTraits::GetPlatformNameString(type, arch));
  ASSERT_TRUE(ext == PlatformTraits::GetArchiveExtension(type, arch));
}

TEST(PackageManagerTest, PlatformConflictLockout) {
  const std::filesystem::path sandboxDir = std::filesystem::current_path() / "test_sandbox";
  std::filesystem::create_directories(sandboxDir);

  // Cleanup sandbox from previous runs if any
  const std::filesystem::path manifestFilePath = sandboxDir / "catupdate.json";
  if (std::filesystem::exists(manifestFilePath)) {
    std::filesystem::remove(manifestFilePath);
  }

  {
    ManifestManager manifest(sandboxDir);
    PackageManager packageManager(manifest);
    NodeJsPackageProvider provider;

    // Register an active installation of nodejs targeting macos-x64
    InstalledPackageState state;
    state.identifier = "nodejs";
    state.targetPlatform = "macos";
    state.targetArchitecture = "x64";
    state.installedVersion = "20.11.1";
    state.installationPath = sandboxDir / "nodejs";
    state.installationDate = "2026-06-21";

    manifest.RegisterOrUpdateInstalledPackage(state);
    ASSERT_TRUE(manifest.IsPackageInstalled("nodejs"));

    // Attempt to install nodejs targeting Windows x64 -> should be BLOCKED (return false)
    bool const winInstallStatus =
        packageManager.InstallPackage(provider, "20.11.1", PlatformType::Windows, ArchitectureType::X64);
    ASSERT_FALSE(winInstallStatus);

    // Attempt to install nodejs targeting macOS ARM64 -> should be BLOCKED (return false)
    bool const armInstallStatus =
        packageManager.InstallPackage(provider, "20.11.1", PlatformType::macOS, ArchitectureType::Arm64);
    ASSERT_FALSE(armInstallStatus);
  }

  // Clean up sandbox
  std::filesystem::remove_all(sandboxDir);
}

} // namespace CatUpdate
