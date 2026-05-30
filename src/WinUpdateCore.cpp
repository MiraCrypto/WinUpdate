#include "WinUpdateCore.hpp"
#include <winhttp.h>
#include <fstream>
#include <iostream>
#include <format>

#include <iostream>

namespace WinUpdate {

void Logger::Log(const std::wstring& msg) {
    std::wcerr << msg << std::endl;
    OutputDebugStringW((msg + L"\n").c_str());
}

std::wstring Utils::ToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string Utils::ToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

bool Downloader::DownloadFile(const std::string& url, const std::filesystem::path& destination, std::function<void(float)> onProgress) {
    // Basic WinHTTP implementation
    // For brevity in this initial implementation, we'll use a simplified synchronous version
    // In a real GUI app, this would be on a separate thread.
    
    HINTERNET hSession = WinHttpOpen(L"WinUpdate/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    // Convert URL to WideString
    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS urlComp = { sizeof(urlComp) };
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring wHost(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring wPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD dwFlags = (urlComp.nPort == INTERNET_DEFAULT_HTTPS_PORT) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    std::ofstream outFile(destination, std::ios::binary);
    
    // Get content length for progress
    DWORD dwContentLength = 0;
    DWORD dwHeaderSize = sizeof(dwContentLength);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwContentLength, &dwHeaderSize, WINHTTP_NO_HEADER_INDEX);

    DWORD dwTotalDownloaded = 0;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;

        char* buffer = new char[dwSize];
        if (WinHttpReadData(hRequest, (LPVOID)buffer, dwSize, &dwDownloaded)) {
            outFile.write(buffer, dwDownloaded);
            dwTotalDownloaded += dwDownloaded;
            if (dwContentLength > 0 && onProgress) {
                onProgress((float)dwTotalDownloaded / (float)dwContentLength);
            }
        }
        delete[] buffer;
    } while (dwSize > 0);

    outFile.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return true;
}

bool Extractor::Extract(const std::filesystem::path& archivePath, const std::filesystem::path& destinationPath) {
    std::filesystem::create_directories(destinationPath);
    
    // Construct tar command: tar -xf "archive" -C "destination"
    // Note: Windows tar.exe supports zip, 7z, tar, etc.
    std::wstring cmd = std::format(L"tar.exe -xf \"{}\" -C \"{}\"", archivePath.wstring(), destinationPath.wstring());
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    if (CreateProcessW(NULL, const_cast<LPWSTR>(cmd.c_str()), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }
    
    return false;
}

ManifestManager::ManifestManager(const std::filesystem::path& rootPath) : m_rootPath(rootPath) {
    Load();
}

void ManifestManager::Load() {
    auto manifestPath = m_rootPath / "winupdate.json";
    if (std::filesystem::exists(manifestPath)) {
        std::ifstream f(manifestPath);
        try {
            nlohmann::json data = nlohmann::json::parse(f);
            for (auto& item : data["packages"]) {
                m_installed.push_back({
                    item["id"],
                    item["version"],
                    item["install_path"],
                    item["install_date"]
                });
            }
        } catch (...) {}
    }
}

void ManifestManager::Save() {
    auto manifestPath = m_rootPath / "winupdate.json";
    nlohmann::json data;
    data["install_path"] = m_rootPath.string();
    data["packages"] = nlohmann::json::array();
    
    for (const auto& pkg : m_installed) {
        data["packages"].push_back({
            {"id", pkg.id},
            {"version", pkg.version},
            {"install_path", pkg.install_path},
            {"install_date", pkg.install_date}
        });
    }
    
    std::ofstream f(manifestPath);
    f << data.dump(4);
}

void ManifestManager::UpdatePackage(const InstalledPackage& pkg) {
    for (auto& item : m_installed) {
        if (item.id == pkg.id) {
            item = pkg;
            Save();
            return;
        }
    }
    m_installed.push_back(pkg);
    Save();
}

} // namespace WinUpdate
