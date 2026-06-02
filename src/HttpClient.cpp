#include "HttpClient.hpp"
#include "ProcessExecutor.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#endif

namespace CatUpdate {

#if defined(_WIN32)

class WindowsHttpClient : public HttpClient {
public:
    bool DownloadFile(
        const UrlString& sourceUrl,
        const std::filesystem::path& destinationFilePath,
        const DownloadProgressCallback& progressCallback
    ) override {
        // Parse components of the URL
        std::wstring wUrl = Utils::ToWString(sourceUrl);
        URL_COMPONENTS urlComponents = { sizeof(urlComponents) };
        urlComponents.dwHostNameLength = static_cast<DWORD>(-1);
        urlComponents.dwUrlPathLength = static_cast<DWORD>(-1);
        urlComponents.dwExtraInfoLength = static_cast<DWORD>(-1);

        if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComponents)) {
            SystemLogger::LogError("WinHttpCrackUrl failed for URL: " + sourceUrl);
            return false;
        }

        std::wstring hostName(urlComponents.lpszHostName, urlComponents.dwHostNameLength);
        std::wstring urlPath(urlComponents.lpszUrlPath, urlComponents.dwUrlPathLength + urlComponents.dwExtraInfoLength);

        HINTERNET sessionHandle = WinHttpOpen(
            L"CatUpdate/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        if (!sessionHandle) {
            SystemLogger::LogError("WinHttpOpen failed.");
            return false;
        }

        HINTERNET connectionHandle = WinHttpConnect(
            sessionHandle,
            hostName.c_str(),
            urlComponents.nPort,
            0
        );
        if (!connectionHandle) {
            WinHttpCloseHandle(sessionHandle);
            SystemLogger::LogError("WinHttpConnect failed.");
            return false;
        }

        DWORD securityFlags = (urlComponents.nPort == INTERNET_DEFAULT_HTTPS_PORT) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET requestHandle = WinHttpOpenRequest(
            connectionHandle,
            L"GET",
            urlPath.c_str(),
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            securityFlags
        );
        if (!requestHandle) {
            WinHttpCloseHandle(connectionHandle);
            WinHttpCloseHandle(sessionHandle);
            SystemLogger::LogError("WinHttpOpenRequest failed.");
            return false;
        }

        // Send the request
        BOOL result = WinHttpSendRequest(
            requestHandle,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0
        );
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
        WinHttpQueryHeaders(
            requestHandle,
            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &contentSize,
            &headerLength,
            WINHTTP_NO_HEADER_INDEX
        );

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
            if (WinHttpReadData(requestHandle, static_cast<LPVOID>(buffer.data()), dataAvailable, &bytesDownloaded)) {
                outputFile.write(buffer.data(), bytesDownloaded);
                totalBytesDownloaded += bytesDownloaded;
                
                if (contentSize > 0 && progressCallback) {
                    float progressPercentage = static_cast<float>(totalBytesDownloaded) / static_cast<float>(contentSize);
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
        URL_COMPONENTS urlComponents = { sizeof(urlComponents) };
        urlComponents.dwHostNameLength = static_cast<DWORD>(-1);
        urlComponents.dwUrlPathLength = static_cast<DWORD>(-1);
        urlComponents.dwExtraInfoLength = static_cast<DWORD>(-1);

        if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComponents)) {
            return "";
        }

        std::wstring hostName(urlComponents.lpszHostName, urlComponents.dwHostNameLength);
        std::wstring urlPath(urlComponents.lpszUrlPath, urlComponents.dwUrlPathLength + urlComponents.dwExtraInfoLength);

        HINTERNET sessionHandle = WinHttpOpen(
            L"CatUpdate/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        if (!sessionHandle) return "";

        HINTERNET connectionHandle = WinHttpConnect(
            sessionHandle,
            hostName.c_str(),
            urlComponents.nPort,
            0
        );
        if (!connectionHandle) {
            WinHttpCloseHandle(sessionHandle);
            return "";
        }

        DWORD securityFlags = (urlComponents.nPort == INTERNET_DEFAULT_HTTPS_PORT) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET requestHandle = WinHttpOpenRequest(
            connectionHandle,
            L"GET",
            urlPath.c_str(),
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            securityFlags
        );
        if (!requestHandle) {
            WinHttpCloseHandle(connectionHandle);
            WinHttpCloseHandle(sessionHandle);
            return "";
        }

        // Set custom User-Agent headers for GitHub Releases API which requires user agent
        std::wstring headers = L"User-Agent: CatUpdate/1.0\r\n";
        WinHttpAddRequestHeaders(
            requestHandle,
            headers.c_str(),
            static_cast<DWORD>(-1),
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE
        );

        BOOL result = WinHttpSendRequest(requestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
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
                if (WinHttpReadData(requestHandle, static_cast<LPVOID>(buffer.data()), dataAvailable, &bytesDownloaded)) {
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

#else // POSIX implementation (Linux / macOS)

class PosixCurlHttpClient : public HttpClient {
public:
    bool DownloadFile(
        const UrlString& sourceUrl,
        const std::filesystem::path& destinationFilePath,
        const DownloadProgressCallback& progressCallback
    ) override {
        // Simply execute: curl -L -s -o <destination> <url>
        // If progress callback is defined, we can execute it silently or simulate 50%-100% steps,
        // or just call curl cleanly. Since curl handles the download progress nicely on screen
        // when in command line, we run it synchronously.
        SystemLogger::LogInformation("Downloading " + sourceUrl + " via curl...");
        
        std::vector<std::string> command = {
            "curl",
            "-L",
            "-s",
            "-o",
            destinationFilePath.string(),
            sourceUrl
        };

        if (progressCallback) {
            progressCallback(0.1f); // Simulate startup
        }

        auto executionResult = ProcessExecutor::ExecuteCommand(command);
        
        if (progressCallback && executionResult && executionResult->exitCode == 0) {
            progressCallback(1.0f); // Finished
        }

        return executionResult.has_value() && executionResult->exitCode == 0;
    }

    std::string FetchStringContent(const UrlString& sourceUrl) override {
        // Run curl to print response to stdout: curl -L -s -A "CatUpdate/1.0" <url>
        std::vector<std::string> command = {
            "curl",
            "-L",
            "-s",
            "-A",
            "CatUpdate/1.0",
            sourceUrl
        };

        auto executionResult = ProcessExecutor::ExecuteCommand(command);
        if (executionResult && executionResult->exitCode == 0) {
            return executionResult->standardOutput;
        }
        
        return "";
    }
};

#endif

std::unique_ptr<HttpClient> HttpClientFactory::CreateDefaultClient() {
#if defined(_WIN32)
    return std::make_unique<WindowsHttpClient>();
#else
    return std::make_unique<PosixCurlHttpClient>();
#endif
}

} // namespace CatUpdate
