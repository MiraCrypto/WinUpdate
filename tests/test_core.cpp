#include "CatUpdateCore.hpp"
#include "test_helper.hpp"
#include <filesystem>
#include <fstream>

namespace CatUpdate {

TEST(PathResolverTest, HomeDirectoryResolution) {
    auto homeDir = PathResolver::GetUserHomeDirectory();
    ASSERT_FALSE(homeDir.empty());
    ASSERT_TRUE(std::filesystem::exists(homeDir) || homeDir == "/");
}

TEST(PathResolverTest, DefaultInstallPathResolution) {
    auto rootPath = PathResolver::GetDefaultInstallationRootPath();
    ASSERT_FALSE(rootPath.empty());
#if defined(_WIN32)
    // Public Documents typically contains "Public" or "Documents"
    std::string pathStr = rootPath.string();
    ASSERT_TRUE(pathStr.find("Public") != std::string::npos || pathStr.find("Documents") != std::string::npos);
#else
    // ~/.local
    ASSERT_TRUE(rootPath.filename() == ".local");
#endif
}

TEST(ManifestManagerTest, SaveAndLoadCycle) {
    // Create localized temporary sandbox inside workspace for deterministic testing
    std::filesystem::path sandboxDir = std::filesystem::current_path() / "test_sandbox";
    std::filesystem::create_directories(sandboxDir);

    // Cleanup sandbox from previous runs if any
    std::filesystem::path manifestFilePath = sandboxDir / "catupdate.json";
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
        ManifestManager manager(sandboxDir);
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
        ManifestManager manager(sandboxDir);
        ASSERT_FALSE(manager.IsPackageInstalled("nodejs"));
    }

    // Clean up sandbox
    std::filesystem::remove_all(sandboxDir);
}

} // namespace CatUpdate
