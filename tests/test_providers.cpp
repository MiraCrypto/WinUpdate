#include "Providers.hpp"
#include "test_helper.hpp"
#include <string>

namespace CatUpdate {

TEST(ProvidersTest, RegistryInitialization) {
  auto providers = PackageProviderRegistry::GetRegisteredProviders();
  ASSERT_FALSE(providers.empty());

  // Check that all 6 providers are registered in the registry
  bool hasNodeJs = false;
  bool hasVscodium = false;
  bool hasPython = false;
  bool hasOpenJdk = false;
  bool hasGit = false;
  bool hasVim = false;

  for (const auto& provider : providers) {
    if (provider->GetIdentifier() == "nodejs") {
      hasNodeJs = true;
    } else if (provider->GetIdentifier() == "vscodium") {
      hasVscodium = true;
    } else if (provider->GetIdentifier() == "python") {
      hasPython = true;
    } else if (provider->GetIdentifier() == "jdk") {
      hasOpenJdk = true;
    } else if (provider->GetIdentifier() == "git") {
      hasGit = true;
    } else if (provider->GetIdentifier() == "vim") {
      hasVim = true;
    }
  }

  ASSERT_TRUE(hasNodeJs);
  ASSERT_TRUE(hasVscodium);
  ASSERT_TRUE(hasPython);
  ASSERT_TRUE(hasOpenJdk);
  ASSERT_TRUE(hasGit);
  ASSERT_TRUE(hasVim);
}

TEST(ProvidersTest, PlatformSupportFiltering) {
  const NodeJsPackageProvider nodeProvider;
  const GitPackageProvider gitProvider;
  const PythonPackageProvider pythonProvider;

  // NodeJs should be supported on all targets
  ASSERT_TRUE(nodeProvider.IsPlatformSupported(PlatformType::Windows, ArchitectureType::X64));
  ASSERT_TRUE(nodeProvider.IsPlatformSupported(PlatformType::macOS, ArchitectureType::Arm64));
  ASSERT_TRUE(nodeProvider.IsPlatformSupported(PlatformType::Linux, ArchitectureType::X64));

  // Git and Python should ONLY be supported on Windows targets
  ASSERT_TRUE(gitProvider.IsPlatformSupported(PlatformType::Windows, ArchitectureType::X64));
  ASSERT_FALSE(gitProvider.IsPlatformSupported(PlatformType::macOS, ArchitectureType::X64));
  ASSERT_FALSE(gitProvider.IsPlatformSupported(PlatformType::Linux, ArchitectureType::Arm64));

  ASSERT_TRUE(pythonProvider.IsPlatformSupported(PlatformType::Windows, ArchitectureType::X64));
  ASSERT_FALSE(pythonProvider.IsPlatformSupported(PlatformType::macOS, ArchitectureType::X64));
  ASSERT_FALSE(pythonProvider.IsPlatformSupported(PlatformType::Linux, ArchitectureType::Arm64));
}

TEST(ProvidersTest, DownloadFilenameFormatting) {
  const NodeJsPackageProvider nodeProvider;

  // Test Windows x64
  std::string const winX64 = nodeProvider.GetArchiveFilename("20.11.1", PlatformType::Windows, ArchitectureType::X64);
  ASSERT_TRUE(winX64 == "node-20.11.1-win-x64.zip");

  // Test Windows ARM64
  std::string const winArm64 =
      nodeProvider.GetArchiveFilename("20.11.1", PlatformType::Windows, ArchitectureType::Arm64);
  ASSERT_TRUE(winArm64 == "node-20.11.1-win-arm64.zip");

  // Test macOS x64
  std::string const macX64 = nodeProvider.GetArchiveFilename("20.11.1", PlatformType::macOS, ArchitectureType::X64);
  ASSERT_TRUE(macX64 == "node-20.11.1-darwin-x64.tar.gz");

  // Test macOS ARM64
  std::string const macArm64 = nodeProvider.GetArchiveFilename("20.11.1", PlatformType::macOS, ArchitectureType::Arm64);
  ASSERT_TRUE(macArm64 == "node-20.11.1-darwin-arm64.tar.gz");

  // Test Linux x64
  std::string const linuxX64 = nodeProvider.GetArchiveFilename("20.11.1", PlatformType::Linux, ArchitectureType::X64);
  ASSERT_TRUE(linuxX64 == "node-20.11.1-linux-x64.tar.xz");

  // Test Linux ARM64
  std::string const linuxArm64 =
      nodeProvider.GetArchiveFilename("20.11.1", PlatformType::Linux, ArchitectureType::Arm64);
  ASSERT_TRUE(linuxArm64 == "node-20.11.1-linux-arm64.tar.xz");
}

} // namespace CatUpdate
