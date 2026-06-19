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

} // namespace CatUpdate
