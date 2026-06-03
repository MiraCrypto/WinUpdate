#pragma once

#include "CatUpdateCore.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace CatUpdate {

using DownloadProgressCallback = std::function<void(float progressPercentage)>;

/**
 * Interface defining the required capabilities for fetching files from the web.
 */
class HttpClient {
public:
  virtual ~HttpClient() = default;

  /**
   * Downloads a file synchronously from a given URL and writes it to the destination path.
   * Invokes the optional progress callback during the download loop.
   */
  virtual bool DownloadFile(const UrlString& sourceUrl,
                            const std::filesystem::path& destinationFilePath,
                            const DownloadProgressCallback& progressCallback = nullptr) = 0;

  /**
   * Fetches JSON metadata from a given web endpoint and returns it as a plain string.
   */
  virtual std::string FetchStringContent(const UrlString& sourceUrl) = 0;
};

/**
 * Factory class to instantiate the best network client for the host operating system.
 */
class HttpClientFactory {
public:
  static std::unique_ptr<HttpClient> CreateDefaultClient();
};

} // namespace CatUpdate
