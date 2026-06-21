#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace CatUpdate {

enum class PlatformType { Windows, Linux, macOS };
enum class ArchitectureType { X64, Arm64 };

class PlatformTraits {
public:
  /**
   * Identifies the current host operating system type.
   */
  static PlatformType GetPlatformType();

  /**
   * Identifies the current host CPU architecture type.
   */
  static ArchitectureType GetHostArchitecture();

  /**
   * Returns the host platform identifier string used by release APIs (e.g. "win-x64", "linux-x64",
   * "darwin-x64").
   */
  static std::string GetPlatformNameString();
  static std::string GetPlatformNameString(PlatformType platform);
  static std::string GetPlatformNameString(PlatformType platform, ArchitectureType arch);

  /**
   * Returns the default compressed package extension for the host (e.g. ".zip", ".tar.xz",
   * ".tar.gz").
   */
  static std::string GetArchiveExtension();
  static std::string GetArchiveExtension(PlatformType platform);
  static std::string GetArchiveExtension(PlatformType platform, ArchitectureType arch);

  /**
   * Returns the command line argument vector required to extract a compressed archive to a target directory.
   */
  static std::vector<std::string> GetExtractionCommand(const std::filesystem::path& archivePath,
                                                       const std::filesystem::path& destinationDirectory);
  static std::vector<std::string> GetExtractionCommand(PlatformType platform, const std::filesystem::path& archivePath,
                                                       const std::filesystem::path& destinationDirectory);
};

} // namespace CatUpdate
