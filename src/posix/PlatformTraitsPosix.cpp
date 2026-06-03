#include "PlatformTraits.hpp"

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

} // namespace CatUpdate
