#pragma once
#include <string>

namespace CatUpdate {

enum class PlatformType {
    Windows,
    Linux,
    macOS
};

class PlatformTraits {
public:
    /**
     * Identifies the current host operating system type.
     */
    static PlatformType GetPlatformType();

    /**
     * Returns the host platform identifier string used by release APIs (e.g. "win-x64", "linux-x64", "darwin-x64").
     */
    static std::string GetPlatformNameString();

    /**
     * Returns the default compressed package extension for the host (e.g. ".zip", ".tar.xz", ".tar.gz").
     */
    static std::string GetArchiveExtension();
};

} // namespace CatUpdate
