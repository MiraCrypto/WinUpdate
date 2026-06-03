#include "CatUpdateCore.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <cstring>

namespace CatUpdate {

// -----------------------------------------------------------------------------
// PathResolver POSIX Implementation
// -----------------------------------------------------------------------------

std::filesystem::path PathResolver::GetDefaultInstallationRootPath() {
    return GetUserHomeDirectory() / ".local";
}

std::filesystem::path PathResolver::GetUserHomeDirectory() {
    const char* homeDirectoryEnv = std::getenv("HOME");
    if (homeDirectoryEnv != nullptr && std::strlen(homeDirectoryEnv) > 0) {
        return std::filesystem::path(homeDirectoryEnv);
    }

    struct passwd* passwordEntry = getpwuid(getuid());
    if (passwordEntry != nullptr && passwordEntry->pw_dir != nullptr) {
        return std::filesystem::path(passwordEntry->pw_dir);
    }

    return std::filesystem::path("/");
}

// -----------------------------------------------------------------------------
// Utils POSIX Implementation (String stubs)
// -----------------------------------------------------------------------------

std::wstring Utils::ToWString(const std::string& utf8Str) {
    return std::wstring(utf8Str.begin(), utf8Str.end());
}

std::string Utils::ToString(const std::wstring& utf16Str) {
    return std::string(utf16Str.begin(), utf16Str.end());
}

} // namespace CatUpdate
