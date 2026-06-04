#include "Providers.hpp"
#include "test_helper.hpp"
#include <algorithm>

namespace CatUpdate {

TEST(ProvidersTest, RegistryInitialization) {
  auto providers = PackageProviderRegistry::GetRegisteredProviders();
  ASSERT_FALSE(providers.empty());

  // Check that NodeJs and VSCodium are present (they are supported on all platforms)
  bool hasNodeJs = false;
  bool hasVscodium = false;
  for (const auto& provider : providers) {
    if (provider->GetIdentifier() == "nodejs") {
      hasNodeJs = true;
    }
    if (provider->GetIdentifier() == "vscodium") {
      hasVscodium = true;
    }
  }

  ASSERT_TRUE(hasNodeJs);
  ASSERT_TRUE(hasVscodium);
}

TEST(ProvidersTest, PlatformSupportFiltering) {
  // NodeJs should always be supported
  const NodeJsPackageProvider nodeProvider;
  ASSERT_TRUE(nodeProvider.IsPlatformSupported());

#ifndef _WIN32
  // Git and Python embeddable should NOT be supported on Linux/macOS
  const GitPackageProvider gitProvider;
  const PythonPackageProvider pythonProvider;

  ASSERT_FALSE(gitProvider.IsPlatformSupported());
  ASSERT_FALSE(pythonProvider.IsPlatformSupported());
#endif
}

TEST(ProvidersTest, DownloadFilenameFormatting) {
  const NodeJsPackageProvider nodeProvider;
  const std::string filename = nodeProvider.GetArchiveFilename("20.11.1");

  ASSERT_FALSE(filename.empty());

#ifdef _WIN32
  ASSERT_TRUE(filename == "node-20.11.1-win-x64.zip");
#elif defined(__APPLE__)
  ASSERT_TRUE(filename == "node-20.11.1-darwin-x64.tar.gz");
#else
  ASSERT_TRUE(filename == "node-20.11.1-linux-x64.tar.xz");
#endif
}

} // namespace CatUpdate
