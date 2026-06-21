#include "PlatformTraits.hpp"

namespace CatUpdate {

std::string PlatformTraits::GetPlatformNameString() {
  return GetPlatformNameString(GetPlatformType());
}

std::string PlatformTraits::GetPlatformNameString(PlatformType platform) {
  if (platform == PlatformType::Windows) {
    return "win-x64";
  }
  if (platform == PlatformType::macOS) {
    return "darwin-x64";
  }
  return "linux-x64";
}

std::string PlatformTraits::GetArchiveExtension() {
  return GetArchiveExtension(GetPlatformType());
}

std::string PlatformTraits::GetArchiveExtension(PlatformType platform) {
  if (platform == PlatformType::Windows) {
    return ".zip";
  }
  if (platform == PlatformType::macOS) {
    return ".tar.gz";
  }
  return ".tar.xz";
}

std::vector<std::string> PlatformTraits::GetExtractionCommand(const std::filesystem::path& archivePath,
                                                              const std::filesystem::path& destinationDirectory) {
  return GetExtractionCommand(GetPlatformType(), archivePath, destinationDirectory);
}

std::vector<std::string> PlatformTraits::GetExtractionCommand(PlatformType platform,
                                                              const std::filesystem::path& archivePath,
                                                              const std::filesystem::path& destinationDirectory) {
  if (platform == PlatformType::Windows) {
    return {"tar.exe", "-xf", archivePath.string(), "-C", destinationDirectory.string()};
  }
  if (archivePath.extension() == ".zip") {
    return {"unzip", "-q", "-o", archivePath.string(), "-d", destinationDirectory.string()};
  }
  return {"tar", "-xf", archivePath.string(), "-C", destinationDirectory.string()};
}

} // namespace CatUpdate
