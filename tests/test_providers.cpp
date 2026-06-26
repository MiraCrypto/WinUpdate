#include "Providers.hpp"
#include "test_helper.hpp"
#include <string>

namespace CatUpdate {

TEST(ProvidersTest, RegistryInitialization) {
  auto providers = PackageProviderRegistry::GetRegisteredProviders();
  ASSERT_FALSE(providers.empty());

  bool hasNodeJs = false;
  bool hasNodeJsLts = false;
  bool hasVscodium = false;
  bool hasVsCode = false;
  bool hasPython = false;
  bool hasOpenJdk = false;
  bool hasOpenJdkLts = false;
  bool hasGit = false;
  bool hasVim = false;

  for (const auto& provider : providers) {
    if (provider->GetIdentifier() == "nodejs") {
      hasNodeJs = true;
    } else if (provider->GetIdentifier() == "nodejs-lts") {
      hasNodeJsLts = true;
    } else if (provider->GetIdentifier() == "vscodium") {
      hasVscodium = true;
    } else if (provider->GetIdentifier() == "vscode") {
      hasVsCode = true;
    } else if (provider->GetIdentifier() == "python") {
      hasPython = true;
    } else if (provider->GetIdentifier() == "jdk") {
      hasOpenJdk = true;
    } else if (provider->GetIdentifier() == "jdk-lts") {
      hasOpenJdkLts = true;
    } else if (provider->GetIdentifier() == "git") {
      hasGit = true;
    } else if (provider->GetIdentifier() == "vim") {
      hasVim = true;
    }
  }

  ASSERT_TRUE(hasNodeJs);
  ASSERT_TRUE(hasNodeJsLts);
  ASSERT_TRUE(hasVscodium);
  ASSERT_TRUE(hasVsCode);
  ASSERT_TRUE(hasPython);
  ASSERT_TRUE(hasOpenJdk);
  ASSERT_TRUE(hasOpenJdkLts);
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

TEST(ProvidersTest, PythonNugetProvider) {
  const PythonPackageProvider pythonProvider;

  // Test that Python specifies "tools" as internal root
  ASSERT_TRUE(pythonProvider.GetArchiveInternalRoot() == "tools");

  // Test Windows x64 NuGet package name and URL
  std::string const winX64Url = pythonProvider.GetDownloadUrl("3.12.3", PlatformType::Windows, ArchitectureType::X64);
  std::string const winX64Filename =
      pythonProvider.GetArchiveFilename("3.12.3", PlatformType::Windows, ArchitectureType::X64);
  ASSERT_TRUE(winX64Url == "https://www.nuget.org/api/v2/package/python/3.12.3");
  ASSERT_TRUE(winX64Filename == "python-3.12.3.nupkg");

  // Test Windows ARM64 NuGet package name and URL
  std::string const winArm64Url =
      pythonProvider.GetDownloadUrl("3.12.3", PlatformType::Windows, ArchitectureType::Arm64);
  std::string const winArm64Filename =
      pythonProvider.GetArchiveFilename("3.12.3", PlatformType::Windows, ArchitectureType::Arm64);
  ASSERT_TRUE(winArm64Url == "https://www.nuget.org/api/v2/package/pythonarm64/3.12.3");
  ASSERT_TRUE(winArm64Filename == "pythonarm64-3.12.3.nupkg");
}

TEST(ProvidersTest, OpenJdkMultiPlatform) {
  const OpenJdkPackageProvider jdk;

  // Windows x64
  ASSERT_TRUE(jdk.GetDownloadUrl("jdk-17.0.1+12", PlatformType::Windows, ArchitectureType::X64) ==
              "https://api.adoptium.net/v3/binary/version/jdk-17.0.1+12/windows/x64/jdk/hotspot/"
              "normal/eclipse");
  ASSERT_TRUE(jdk.GetArchiveFilename("jdk-17.0.1+12", PlatformType::Windows, ArchitectureType::X64) ==
              "openjdk-jdk-17.0.1+12-x64.zip");

  // macOS ARM64
  ASSERT_TRUE(jdk.GetDownloadUrl("jdk-17.0.1+12", PlatformType::macOS, ArchitectureType::Arm64) ==
              "https://api.adoptium.net/v3/binary/version/jdk-17.0.1+12/mac/aarch64/jdk/hotspot/"
              "normal/eclipse");
  ASSERT_TRUE(jdk.GetArchiveFilename("jdk-17.0.1+12", PlatformType::macOS, ArchitectureType::Arm64) ==
              "openjdk-jdk-17.0.1+12-aarch64.tar.gz");

  // Linux ARM64
  ASSERT_TRUE(jdk.GetDownloadUrl("jdk-17.0.1+12", PlatformType::Linux, ArchitectureType::Arm64) ==
              "https://api.adoptium.net/v3/binary/version/jdk-17.0.1+12/linux/aarch64/jdk/hotspot/"
              "normal/eclipse");
  ASSERT_TRUE(jdk.GetArchiveFilename("jdk-17.0.1+12", PlatformType::Linux, ArchitectureType::Arm64) ==
              "openjdk-jdk-17.0.1+12-aarch64.tar.gz");
}

TEST(ProvidersTest, VSCodeMultiPlatform) {
  const VSCodePackageProvider vscode;

  // Windows x64
  ASSERT_TRUE(vscode.GetDownloadUrl("1.85.0", PlatformType::Windows, ArchitectureType::X64) ==
              "https://update.code.visualstudio.com/1.85.0/win32-x64-archive/stable");
  ASSERT_TRUE(vscode.GetArchiveFilename("1.85.0", PlatformType::Windows, ArchitectureType::X64) ==
              "vscode-win32-x64-archive-1.85.0.zip");

  // macOS ARM64
  ASSERT_TRUE(vscode.GetDownloadUrl("1.85.0", PlatformType::macOS, ArchitectureType::Arm64) ==
              "https://update.code.visualstudio.com/1.85.0/darwin-arm64/stable");
  ASSERT_TRUE(vscode.GetArchiveFilename("1.85.0", PlatformType::macOS, ArchitectureType::Arm64) ==
              "vscode-darwin-arm64-1.85.0.zip");
}

} // namespace CatUpdate
