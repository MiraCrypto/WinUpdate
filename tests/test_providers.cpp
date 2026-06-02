#include "Providers.hpp"
#include "test_helper.hpp"
#include <algorithm>

namespace CatUpdate {

TEST(ProvidersTest, RegistryInitialization) {
    auto providers = PackageProviderRegistry::GetRegisteredProviders();
    ASSERT_FALSE(providers.empty());

    // Check that NodeJs and VSCodium are present (they are supported on all platforms)
    bool hasNode = false;
    bool hasVsc = false;
    for (const auto& p : providers) {
        if (p->GetIdentifier() == "nodejs") hasNode = true;
        if (p->GetIdentifier() == "vscodium") hasVsc = true;
    }

    ASSERT_TRUE(hasNode);
    ASSERT_TRUE(hasVsc);
}

TEST(ProvidersTest, PlatformSupportFiltering) {
    // NodeJs should always be supported
    NodeJsPackageProvider nodeProvider;
    ASSERT_TRUE(nodeProvider.IsPlatformSupported());

#if !defined(_WIN32)
    // Git and Python embeddable should NOT be supported on Linux/macOS
    GitPackageProvider gitProvider;
    PythonPackageProvider pythonProvider;
    
    ASSERT_FALSE(gitProvider.IsPlatformSupported());
    ASSERT_FALSE(pythonProvider.IsPlatformSupported());
#endif
}

TEST(ProvidersTest, DownloadFilenameFormatting) {
    NodeJsPackageProvider nodeProvider;
    std::string filename = nodeProvider.GetArchiveFilename("20.11.1");
    
    ASSERT_FALSE(filename.empty());
    
#if defined(_WIN32)
    ASSERT_TRUE(filename == "node-20.11.1-win-x64.zip");
#elif defined(__APPLE__)
    ASSERT_TRUE(filename == "node-20.11.1-darwin-x64.tar.gz");
#else
    ASSERT_TRUE(filename == "node-20.11.1-linux-x64.tar.xz");
#endif
}

} // namespace CatUpdate
