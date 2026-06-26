#include "CatUpdateCore.hpp"
#include <cstddef>
#include <filesystem>
#include <minwindef.h>
#include <string>
#include <stringapiset.h>
#include <winerror.h>
#include <winnls.h>
#include <winnt.h>

#define WIN32_LEAN_AND_MEAN
#include <shlobj.h>

namespace CatUpdate {

// -----------------------------------------------------------------------------
// PathResolver Windows Implementation
// -----------------------------------------------------------------------------

std::filesystem::path PathResolver::GetDefaultInstallationRootPath() {
  WCHAR publicDocumentsPath[MAX_PATH];
  HRESULT const result =
      SHGetFolderPathW(nullptr, CSIDL_COMMON_DOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, publicDocumentsPath);
  if (SUCCEEDED(result)) {
    return std::filesystem::path(publicDocumentsPath);
  }
  return std::filesystem::path(R"(C:\Users\Public\Documents)");
}

std::filesystem::path PathResolver::GetUserHomeDirectory() {
  WCHAR userProfilePath[MAX_PATH];
  HRESULT const result = SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, userProfilePath);
  if (SUCCEEDED(result)) {
    return std::filesystem::path(userProfilePath);
  }
  return std::filesystem::path("C:\\Users\\Default");
}

std::filesystem::path PathResolver::GetLocalAppDirectory() {
  WCHAR localAppDataPath[MAX_PATH];
  HRESULT const result = SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppDataPath);
  if (SUCCEEDED(result)) {
    return std::filesystem::path(localAppDataPath);
  }
  return GetUserHomeDirectory() / "AppData" / "Local";
}

std::filesystem::path PathResolver::GetUserDocumentsDirectory() {
  WCHAR userDocumentsPath[MAX_PATH];
  HRESULT const result = SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, userDocumentsPath);
  if (SUCCEEDED(result)) {
    return std::filesystem::path(userDocumentsPath);
  }
  return GetUserHomeDirectory() / "Documents";
}

// -----------------------------------------------------------------------------
// Utils Windows Implementation (MultiByteToWideChar conversions)
// -----------------------------------------------------------------------------

std::wstring Utils::ToWString(const std::string& utf8Str) {
  if (utf8Str.empty()) {
    return L"";
  }
  int const sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), static_cast<int>(utf8Str.size()), nullptr, 0);
  std::wstring wstrTo(sizeNeeded, 0);
  MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), static_cast<int>(utf8Str.size()), wstrTo.data(), sizeNeeded);
  return wstrTo;
}

std::string Utils::ToString(const std::wstring& utf16Str) {
  if (utf16Str.empty()) {
    return "";
  }
  int const sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), static_cast<int>(utf16Str.size()), nullptr,
                                             0, nullptr, nullptr);
  std::string strTo(sizeNeeded, 0);
  WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), static_cast<int>(utf16Str.size()), strTo.data(), sizeNeeded,
                      nullptr, nullptr);
  return strTo;
}

} // namespace CatUpdate
