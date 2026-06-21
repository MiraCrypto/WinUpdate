#include "PlatformTraits.hpp"
#include <string>

namespace CatUpdate {

PlatformType PlatformTraits::GetPlatformType() {
#ifdef __APPLE__
  return PlatformType::macOS;
#else
  return PlatformType::Linux;
#endif
}

std::string PlatformTraits::GetPlatformNameString() {
#ifdef __APPLE__
  return "darwin-x64";
#else
  return "linux-x64";
#endif
}

std::string PlatformTraits::GetArchiveExtension() {
#ifdef __APPLE__
  return ".tar.gz";
#else
  return ".tar.xz";
#endif
}

std::vector<std::string> PlatformTraits::GetExtractionCommand(const std::filesystem::path& archivePath,
                                                              const std::filesystem::path& destinationDirectory) {
  if (archivePath.extension() == ".zip") {
    return {"unzip", "-q", "-o", archivePath.string(), "-d", destinationDirectory.string()};
  }
  return {"tar", "-xf", archivePath.string(), "-C", destinationDirectory.string()};
}

} // namespace CatUpdate
