#include "PlatformTraits.hpp"
#include <string>

namespace CatUpdate {

PlatformType PlatformTraits::GetPlatformType() {
  return PlatformType::Windows;
}

std::string PlatformTraits::GetPlatformNameString() {
  return "win-x64";
}

std::string PlatformTraits::GetArchiveExtension() {
  return ".zip";
}

std::vector<std::string> PlatformTraits::GetExtractionCommand(const std::filesystem::path& archivePath,
                                                              const std::filesystem::path& destinationDirectory) {
  return {"tar.exe", "-xf", archivePath.string(), "-C", destinationDirectory.string()};
}

} // namespace CatUpdate
