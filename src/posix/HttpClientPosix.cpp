#include "HttpClient.hpp"
#include "CatUpdateCore.hpp"
#include "ProcessExecutor.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace CatUpdate {

namespace {
class PosixCurlHttpClient : public HttpClient {
public:
  bool DownloadFile(const UrlString& sourceUrl, const std::filesystem::path& destinationFilePath,
                    const DownloadProgressCallback& progressCallback) override {
    SystemLogger::LogInformation("Downloading " + sourceUrl + " via curl...");

    const std::vector<std::string> command = {"curl", "-L", "-s", "-o", destinationFilePath.string(), sourceUrl};

    if (progressCallback) {
      progressCallback(0.1F); // Simulate startup
    }

    auto executionResult = ProcessExecutor::ExecuteCommand(command);

    if (progressCallback && executionResult && executionResult->exitCode == 0) {
      progressCallback(1.0F); // Finished
    }

    return executionResult.has_value() && executionResult->exitCode == 0;
  }

  std::string FetchStringContent(const UrlString& sourceUrl) override {
    const std::vector<std::string> command = {"curl", "-L", "-s", "-A", "CatUpdate/1.0", sourceUrl};

    auto executionResult = ProcessExecutor::ExecuteCommand(command);
    if (executionResult && executionResult->exitCode == 0) {
      return executionResult->standardOutput;
    }

    return "";
  }
};
} // namespace

std::unique_ptr<HttpClient> HttpClientFactory::CreateDefaultClient() {
  return std::make_unique<PosixCurlHttpClient>();
}

} // namespace CatUpdate
