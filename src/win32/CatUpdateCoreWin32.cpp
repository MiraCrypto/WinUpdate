#include "CatUpdateCore.hpp"

#define WIN32_LEAN_AND_MEAN
#include <shlobj.h>
#include <windows.h>

namespace CatUpdate {

// -----------------------------------------------------------------------------
// PathResolver Windows Implementation
// -----------------------------------------------------------------------------

std::filesystem::path PathResolver::GetDefaultInstallationRootPath() {
  WCHAR publicDocumentsPath[MAX_PATH];
  HRESULT result = SHGetFolderPathW(NULL, CSIDL_COMMON_DOCUMENTS, NULL, SHGFP_TYPE_CURRENT, publicDocumentsPath);
  if (SUCCEEDED(result)) {
    return std::filesystem::path(publicDocumentsPath);
  }
  return std::filesystem::path(R"(C:\Users\Public\Documents)");
}

std::filesystem::path PathResolver::GetUserHomeDirectory() {
  WCHAR userProfilePath[MAX_PATH];
  HRESULT result = SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, userProfilePath);
  if (SUCCEEDED(result)) {
    return std::filesystem::path(userProfilePath);
  }
  return std::filesystem::path("C:\\Users\\Default");
}

// -----------------------------------------------------------------------------
// Utils Windows Implementation (MultiByteToWideChar conversions)
// -----------------------------------------------------------------------------

std::wstring Utils::ToWString(const std::string& utf8Str) {
  if (utf8Str.empty()) {
    return L"";
  }
  int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), static_cast<int>(utf8Str.size()), NULL, 0);
  std::wstring wstrTo(sizeNeeded, 0);
  MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), static_cast<int>(utf8Str.size()), wstrTo.data(), sizeNeeded);
  return wstrTo;
}

std::string Utils::ToString(const std::wstring& utf16Str) {
  if (utf16Str.empty()) {
    return "";
  }
  int sizeNeeded =
      WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), static_cast<int>(utf16Str.size()), NULL, 0, NULL, NULL);
  std::string strTo(sizeNeeded, 0);
  WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), static_cast<int>(utf16Str.size()), strTo.data(), sizeNeeded, NULL,
                      NULL);
  return strTo;
}

} // namespace CatUpdate
