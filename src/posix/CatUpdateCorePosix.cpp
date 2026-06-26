#include "CatUpdateCore.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <pwd.h>
#include <string>
#include <unistd.h>

namespace CatUpdate {

// -----------------------------------------------------------------------------
// PathResolver POSIX Implementation
// -----------------------------------------------------------------------------

std::filesystem::path PathResolver::GetDefaultInstallationRootPath() {
  return GetUserHomeDirectory() / ".local";
}

std::filesystem::path PathResolver::GetUserHomeDirectory() {
  const char* const homeDirectoryEnv = std::getenv("HOME");
  if (homeDirectoryEnv != nullptr && std::strlen(homeDirectoryEnv) > 0) {
    return {homeDirectoryEnv};
  }

  const struct passwd* const passwordEntry = getpwuid(getuid());
  if (passwordEntry != nullptr && passwordEntry->pw_dir != nullptr) {
    return {passwordEntry->pw_dir};
  }

  return {"/"};
}

std::filesystem::path PathResolver::GetLocalAppDirectory() {
  return GetUserHomeDirectory() / ".local" / "share";
}

std::filesystem::path PathResolver::GetUserDocumentsDirectory() {
  return GetUserHomeDirectory() / "Documents";
}

// -----------------------------------------------------------------------------
// Utils POSIX Implementation (String stubs)
// -----------------------------------------------------------------------------

std::wstring Utils::ToWString(const std::string& utf8Str) {
  return {utf8Str.begin(), utf8Str.end()};
}

std::string Utils::ToString(const std::wstring& utf16Str) {
  return {utf16Str.begin(), utf16Str.end()};
}

} // namespace CatUpdate
