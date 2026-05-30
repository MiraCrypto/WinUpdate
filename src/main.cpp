#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <filesystem>
#include <format>
#include "WinUpdateCore.hpp"
#include "Providers.hpp"

using namespace WinUpdate;

// Global state
HINSTANCE hInst;
HWND hWndMain;
HWND hListView;
HWND hComboVersion;
HWND hBtnInstall;
HWND hBtnPath;
HWND hStatus;

std::unique_ptr<ManifestManager> g_manifest;
std::vector<std::unique_ptr<PackageProvider>> g_providers;
int g_selectedPackageIndex = -1;

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitializeProviders();
void UpdatePackageList();
void OnPackageSelect();
void StartInstall();

// Clean wWinMain signature
extern "C" int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    
    // Initialize common controls for modern UI look
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszClassName = L"WinUpdateMain";
    RegisterClassExW(&wcex);

    hWndMain = CreateWindowW(L"WinUpdateMain", L"WinUpdate - Software Manager", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 700, 500, nullptr, nullptr, hInstance, nullptr);

    if (!hWndMain) return FALSE;

    InitializeProviders();
    Logger::Log(L"Providers initialized.");

    // Default path: Public Documents / WinUpdate
    WCHAR path[MAX_PATH];
    SHGetSpecialFolderPathW(NULL, path, CSIDL_COMMON_DOCUMENTS, FALSE);
    Logger::Log(L"Base path fetched.");

    std::filesystem::path rootPath = std::filesystem::path(path) / "WinUpdate";
    Logger::Log(L"Root path: " + rootPath.wstring());

    g_manifest = std::make_unique<ManifestManager>(rootPath);
    Logger::Log(L"Manifest manager initialized.");

    UpdatePackageList();

    ShowWindow(hWndMain, nCmdShow);
    UpdateWindow(hWndMain);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

void OnChangePath();

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        hListView = CreateWindowExW(0, WC_LISTVIEW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
            10, 10, 660, 300, hWnd, (HMENU)1001, hInst, NULL);
        ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText = (LPWSTR)L"Software"; lvc.cx = 200; ListView_InsertColumn(hListView, 0, &lvc);
        lvc.pszText = (LPWSTR)L"Installed Version"; lvc.cx = 150; ListView_InsertColumn(hListView, 1, &lvc);
        lvc.pszText = (LPWSTR)L"Status"; lvc.cx = 150; ListView_InsertColumn(hListView, 2, &lvc);

        hComboVersion = CreateWindowExW(0, WC_COMBOBOX, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            10, 320, 200, 200, hWnd, (HMENU)1002, hInst, NULL);

        hBtnInstall = CreateWindowExW(0, WC_BUTTON, L"Install / Update", WS_CHILD | WS_VISIBLE | WS_DISABLED,
            220, 320, 150, 25, hWnd, (HMENU)1003, hInst, NULL);

        hBtnPath = CreateWindowExW(0, WC_BUTTON, L"Change Path", WS_CHILD | WS_VISIBLE,
            380, 320, 120, 25, hWnd, (HMENU)1004, hInst, NULL);

        hStatus = CreateWindowExW(0, WC_STATIC, L"Ready", WS_CHILD | WS_VISIBLE,
            10, 420, 660, 20, hWnd, NULL, hInst, NULL);
        break;
    }
    case WM_NOTIFY: {
        NMHDR* pnmh = (NMHDR*)lParam;
        if (pnmh->idFrom == 1001 && pnmh->code == LVN_ITEMCHANGED) {
            OnPackageSelect();
        }
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 1003) StartInstall();
        if (LOWORD(wParam) == 1004) OnChangePath();
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void InitializeProviders() {
    g_providers.push_back(std::make_unique<NodeJsProvider>());
    g_providers.push_back(std::make_unique<VSCodiumProvider>());
    g_providers.push_back(std::make_unique<PythonProvider>());
    g_providers.push_back(std::make_unique<JdkProvider>());
    g_providers.push_back(std::make_unique<GitProvider>());
    g_providers.push_back(std::make_unique<VimProvider>());
}

void UpdatePackageList() {
    ListView_DeleteAllItems(hListView);
    auto installed = g_manifest->GetInstalledPackages();

    for (size_t i = 0; i < g_providers.size(); ++i) {
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        
        std::wstring name = Utils::ToWString(g_providers[i]->GetName());
        lvi.pszText = (LPWSTR)name.c_str();
        ListView_InsertItem(hListView, &lvi);

        std::wstring ver = L"Not Installed";
        for (const auto& pkg : installed) {
            if (pkg.id == g_providers[i]->GetId()) {
                ver = Utils::ToWString(pkg.version);
                break;
            }
        }
        ListView_SetItemText(hListView, (int)i, 1, (LPWSTR)ver.c_str());
        ListView_SetItemText(hListView, (int)i, 2, (LPWSTR)L"-");
    }
}

void OnPackageSelect() {
    int idx = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if (idx == -1 || idx == g_selectedPackageIndex) return;
    g_selectedPackageIndex = idx;

    SetWindowTextW(hStatus, L"Fetching versions...");
    EnableWindow(hBtnInstall, FALSE);
    SendMessage(hComboVersion, CB_RESETCONTENT, 0, 0);

    // Fetch versions in background
    std::thread([idx]() {
        auto versions = g_providers[idx]->FetchVersions();
        
        // Populate combo on UI thread (simplified)
        for (const auto& v : versions) {
            std::wstring wv = Utils::ToWString(v);
            SendMessage(hComboVersion, CB_ADDSTRING, 0, (LPARAM)wv.c_str());
        }
        SendMessage(hComboVersion, CB_SETCURSEL, 0, 0);
        EnableWindow(hBtnInstall, TRUE);
        SetWindowTextW(hStatus, L"Versions loaded.");
    }).detach();
}

void StartInstall() {
    int pkgIdx = g_selectedPackageIndex;
    int verIdx = (int)SendMessage(hComboVersion, CB_GETCURSEL, 0, 0);
    if (pkgIdx == -1 || verIdx == -1) return;

    WCHAR buffer[256];
    SendMessage(hComboVersion, CB_GETLBTEXT, verIdx, (LPARAM)buffer);
    std::wstring wVer(buffer);
    std::string ver = Utils::ToString(wVer);

    SetWindowTextW(hStatus, L"Downloading...");
    EnableWindow(hBtnInstall, FALSE);

    std::thread([pkgIdx, ver]() {
        auto provider = g_providers[pkgIdx].get();
        auto url = provider->GetDownloadUrl(ver);
        auto destDir = g_manifest->GetRootPath() / provider->GetId();
        auto tempZip = g_manifest->GetRootPath() / "temp_archive";

        std::filesystem::create_directories(g_manifest->GetRootPath());

        if (Downloader::DownloadFile(url, tempZip, [](float progress) {
            std::wstring status = std::format(L"Downloading... {:.1f}%", progress * 100);
            SetWindowTextW(hStatus, status.c_str());
        })) {
            SetWindowTextW(hStatus, L"Extracting...");
            if (Extractor::Extract(tempZip, destDir)) {
                std::filesystem::remove(tempZip);
                
                InstalledPackage pkg;
                pkg.id = provider->GetId();
                pkg.version = ver;
                pkg.install_path = destDir.string();
                pkg.install_date = "2026-05-30";
                
                g_manifest->UpdatePackage(pkg);
                
                SetWindowTextW(hStatus, L"Installation Complete!");
                UpdatePackageList();
            } else {
                SetWindowTextW(hStatus, L"Extraction Failed!");
            }
        } else {
            SetWindowTextW(hStatus, L"Download Failed!");
        }
        EnableWindow(hBtnInstall, TRUE);
    }).detach();
}

void OnChangePath() {
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hWndMain;
    bi.lpszTitle = L"Select Installation Directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != nullptr) {
        WCHAR path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            std::filesystem::path newPath(path);
            g_manifest = std::make_unique<ManifestManager>(newPath);
            UpdatePackageList();
            SetWindowTextW(hStatus, L"Path updated.");
        }
        CoTaskMemFree(pidl);
    }
}
