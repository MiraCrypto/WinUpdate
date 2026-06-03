#include "CatUpdateCore.hpp"
#include "HttpClient.hpp"
#include <fstream>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

namespace CatUpdate {

class WindowsHttpClient : public HttpClient {
public:
  bool DownloadFile(const UrlString& sourceUrl, const std::filesystem::path& destinationFilePath,
                    const DownloadProgressCallback& progressCallback) override {
    // Parse components of the URL using Unicode utility
    std::wstring wUrl = Utils::ToWString(sourceUrl);
    URL_COMPONENTS urlComponents = {sizeof(urlComponents)};
    urlComponents.dwHostNameLength = static_cast<DWORD>(-1);
    urlComponents.dwUrlPathLength = static_cast<DWORD>(-1);
    urlComponents.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComponents)) {
      SystemLogger::LogError("WinHttpCrackUrl failed for URL: " + sourceUrl);
      return false;
    }

    std::wstring hostName(urlComponents.lpszHostName, urlComponents.dwHostNameLength);
    std::wstring urlPath(urlComponents.lpszUrlPath,
                         urlComponents.dwUrlPathLength + urlComponents.dwExtraInfoLength);

    HINTERNET sessionHandle = WinHttpOpen(L"CatUpdate/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sessionHandle) {
      SystemLogger::LogError("WinHttpOpen failed.");
      return false;
    }

    HINTERNET connectionHandle =
        WinHttpConnect(sessionHandle, hostName.c_str(), urlComponents.nPort, 0);
    if (!connectionHandle) {
      WinHttpCloseHandle(sessionHandle);
      SystemLogger::LogError("WinHttpConnect failed.");
      return false;
    }

    DWORD securityFlags =
        (urlComponents.nPort == INTERNET_DEFAULT_HTTPS_PORT) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET requestHandle =
        WinHttpOpenRequest(connectionHandle, L"GET", urlPath.c_str(), NULL, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, securityFlags);
    if (!requestHandle) {
      WinHttpCloseHandle(connectionHandle);
      WinHttpCloseHandle(sessionHandle);
      SystemLogger::LogError("WinHttpOpenRequest failed.");
      return false;
    }

    // Send the request
    BOOL result = WinHttpSendRequest(requestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!result) {
      WinHttpCloseHandle(requestHandle);
      WinHttpCloseHandle(connectionHandle);
      WinHttpCloseHandle(sessionHandle);
      SystemLogger::LogError("WinHttpSendRequest failed.");
      return false;
    }

    // Receive the response
    result = WinHttpReceiveResponse(requestHandle, NULL);
    if (!result) {
      WinHttpCloseHandle(requestHandle);
      WinHttpCloseHandle(connectionHandle);
      WinHttpCloseHandle(sessionHandle);
      SystemLogger::LogError("WinHttpReceiveResponse failed.");
      return false;
    }

    // Query Content-Length for progress tracking
    DWORD contentSize = 0;
    DWORD headerLength = sizeof(contentSize);
    WinHttpQueryHeaders(requestHandle, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &contentSize, &headerLength,
                        WINHTTP_NO_HEADER_INDEX);

    // Open destination file
    std::ofstream outputFile(destinationFilePath, std::ios::binary);
    if (!outputFile.is_open()) {
      WinHttpCloseHandle(requestHandle);
      WinHttpCloseHandle(connectionHandle);
      WinHttpCloseHandle(sessionHandle);
      SystemLogger::LogError("Failed to open destination file: " + destinationFilePath.string());
      return false;
    }

    // Buffer download loop
    DWORD dataAvailable = 0;
    DWORD bytesDownloaded = 0;
    DWORD totalBytesDownloaded = 0;

    do {
      if (!WinHttpQueryDataAvailable(requestHandle, &dataAvailable)) {
        break;
      }
      if (dataAvailable == 0) {
        break;
      }

      std::vector<char> buffer(dataAvailable);
      if (WinHttpReadData(requestHandle, static_cast<LPVOID>(buffer.data()), dataAvailable,
                          &bytesDownloaded)) {
        outputFile.write(buffer.data(), bytesDownloaded);
        totalBytesDownloaded += bytesDownloaded;

        if (contentSize > 0 && progressCallback) {
          float progressPercentage =
              static_cast<float>(totalBytesDownloaded) / static_cast<float>(contentSize);
          progressCallback(progressPercentage);
        }
      }
    } while (dataAvailable > 0);

    outputFile.close();
    WinHttpCloseHandle(requestHandle);
    WinHttpCloseHandle(connectionHandle);
    WinHttpCloseHandle(sessionHandle);

    return true;
  }

  std::string FetchStringContent(const UrlString& sourceUrl) override {
    std::wstring wUrl = Utils::ToWString(sourceUrl);
    URL_COMPONENTS urlComponents = {sizeof(urlComponents)};
    urlComponents.dwHostNameLength = static_cast<DWORD>(-1);
    urlComponents.dwUrlPathLength = static_cast<DWORD>(-1);
    urlComponents.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComponents)) {
      return "";
    }

    std::wstring hostName(urlComponents.lpszHostName, urlComponents.dwHostNameLength);
    std::wstring urlPath(urlComponents.lpszUrlPath,
                         urlComponents.dwUrlPathLength + urlComponents.dwExtraInfoLength);

    HINTERNET sessionHandle = WinHttpOpen(L"CatUpdate/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sessionHandle) {
      return "";
    }

    HINTERNET connectionHandle =
        WinHttpConnect(sessionHandle, hostName.c_str(), urlComponents.nPort, 0);
    if (!connectionHandle) {
      WinHttpCloseHandle(sessionHandle);
      return "";
    }

    DWORD securityFlags =
        (urlComponents.nPort == INTERNET_DEFAULT_HTTPS_PORT) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET requestHandle =
        WinHttpOpenRequest(connectionHandle, L"GET", urlPath.c_str(), NULL, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, securityFlags);
    if (!requestHandle) {
      WinHttpCloseHandle(connectionHandle);
      WinHttpCloseHandle(sessionHandle);
      return "";
    }

    // Set custom User-Agent headers for GitHub Releases API
    std::wstring headers = L"User-Agent: CatUpdate/1.0\r\n";
    WinHttpAddRequestHeaders(requestHandle, headers.c_str(), static_cast<DWORD>(-1),
                             WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    BOOL result = WinHttpSendRequest(requestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (result) {
      result = WinHttpReceiveResponse(requestHandle, NULL);
    }

    std::string responseString;
    if (result) {
      DWORD dataAvailable = 0;
      DWORD bytesDownloaded = 0;
      do {
        if (!WinHttpQueryDataAvailable(requestHandle, &dataAvailable) || dataAvailable == 0) {
          break;
        }
        std::vector<char> buffer(dataAvailable);
        if (WinHttpReadData(requestHandle, static_cast<LPVOID>(buffer.data()), dataAvailable,
                            &bytesDownloaded)) {
          responseString.append(buffer.data(), bytesDownloaded);
        }
      } while (dataAvailable > 0);
    }

    WinHttpCloseHandle(requestHandle);
    WinHttpCloseHandle(connectionHandle);
    WinHttpCloseHandle(sessionHandle);

    return responseString;
  }
};

std::unique_ptr<HttpClient> HttpClientFactory::CreateDefaultClient() {
  return std::make_unique<WindowsHttpClient>();
}

} // namespace CatUpdate
