#include "PlatformTraits.hpp"

namespace CatUpdate {

ArchitectureType PlatformTraits::GetHostArchitecture() {
#if defined(__aarch64__) || defined(_M_ARM64)
  return ArchitectureType::Arm64;
#else
  return ArchitectureType::X64;
#endif
}

std::string PlatformTraits::GetPlatformNameString() {
  return GetPlatformNameString(GetPlatformType(), GetHostArchitecture());
}

std::string PlatformTraits::GetPlatformNameString(PlatformType platform) {
  return GetPlatformNameString(platform, GetHostArchitecture());
}

std::string PlatformTraits::GetPlatformNameString(PlatformType platform, ArchitectureType arch) {
  std::string osStr;
  if (platform == PlatformType::Windows) {
    osStr = "win";
  } else if (platform == PlatformType::macOS) {
    osStr = "darwin";
  } else {
    osStr = "linux";
  }

  std::string const archStr = (arch == ArchitectureType::Arm64) ? "arm64" : "x64";
  return osStr + "-" + archStr;
}

std::string PlatformTraits::GetArchiveExtension() {
  return GetArchiveExtension(GetPlatformType(), GetHostArchitecture());
}

std::string PlatformTraits::GetArchiveExtension(PlatformType platform) {
  return GetArchiveExtension(platform, GetHostArchitecture());
}

std::string PlatformTraits::GetArchiveExtension(PlatformType platform, ArchitectureType /*arch*/) {
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
