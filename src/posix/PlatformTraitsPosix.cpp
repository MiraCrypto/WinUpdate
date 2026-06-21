#include "PlatformTraits.hpp"

namespace CatUpdate {

PlatformType PlatformTraits::GetPlatformType() {
#ifdef __APPLE__
  return PlatformType::macOS;
#else
  return PlatformType::Linux;
#endif
}

} // namespace CatUpdate
