#include "HttpClient.hpp"
#include "ProcessExecutor.hpp"
#include <vector>

namespace CatUpdate {

class PosixCurlHttpClient : public HttpClient {
public:
  bool DownloadFile(const UrlString& sourceUrl, const std::filesystem::path& destinationFilePath,
                    const DownloadProgressCallback& progressCallback) override {
    SystemLogger::LogInformation("Downloading " + sourceUrl + " via curl...");

    std::vector<std::string> command = {"curl",   "-L", "-s", "-o", destinationFilePath.string(),
                                        sourceUrl};

    constexpr float STARTUP_PROGRESS_FRACTION = 0.1F;
    constexpr float COMPLETED_PROGRESS_FRACTION = 1.0F;

    if (progressCallback) {
      progressCallback(STARTUP_PROGRESS_FRACTION); // Simulate startup
    }

    auto executionResult = ProcessExecutor::ExecuteCommand(command);

    if (progressCallback && executionResult && executionResult->exitCode == 0) {
      progressCallback(COMPLETED_PROGRESS_FRACTION); // Finished
    }

    return executionResult.has_value() && executionResult->exitCode == 0;
  }

  std::string FetchStringContent(const UrlString& sourceUrl) override {
    std::vector<std::string> command = {"curl", "-L", "-s", "-A", "CatUpdate/1.0", sourceUrl};

    auto executionResult = ProcessExecutor::ExecuteCommand(command);
    if (executionResult && executionResult->exitCode == 0) {
      return executionResult->standardOutput;
    }

    return "";
  }
};

std::unique_ptr<HttpClient> HttpClientFactory::CreateDefaultClient() {
  return std::make_unique<PosixCurlHttpClient>();
}

} // namespace CatUpdate
