#include "CatUpdateCore.hpp"
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <minwindef.h>
#include <string>
#include <utility>
#include <vector>
#include <windef.h>
#include <wingdi.h>
#include <winnt.h>
#ifdef _WIN32

#define WIN32_WINNT 0x0A00
#define WIN32_IE 0x0700
#include <windows.h>

#include <commctrl.h>
#include <objbase.h>
#include <shlobj.h>

#include "HttpClient.hpp"
#include "ProcessExecutor.hpp"
#include "Providers.hpp"
#include "Ui.hpp"
#include <chrono>
#include <cmath>
#include <thread>

namespace CatUpdate {

// -----------------------------------------------------------------------------
// Win32 Entry Point & Class Registration
// -----------------------------------------------------------------------------
// ... Rest of entry point code ...

int DesktopUserInterface::Run(HINSTANCE hinstance, int cmdShow) {
  m_hinstance = hinstance;

  // Initialize Common Controls
  INITCOMMONCONTROLSEX commonControlsInitEx;
  commonControlsInitEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  commonControlsInitEx.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&commonControlsInitEx);

  // Register the window class
  WNDCLASSEXW windowClass = {.cbSize = sizeof(WNDCLASSEXW)};
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = MainWndProc;
  windowClass.hInstance = hinstance;
  windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);

  // Dark Space Background brush
  m_backgroundBrush = CreateSolidBrush(RGB(6, 6, 14));
  windowClass.hbrBackground = m_backgroundBrush;
  windowClass.lpszClassName = L"CatUpdateMainWindowClass";

  if (RegisterClassExW(&windowClass) == 0u) {
    SystemLogger::LogError("Failed to register main Win32 window class.");
    return 1;
  }

  // Create the Main Window
  m_mainWindow = CreateWindowExW(WS_EX_APPWINDOW, L"CatUpdateMainWindowClass", L"CatUpdate - No-Admin Software Center",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
                                 800, 650, nullptr, nullptr, hinstance, nullptr);

  if (m_mainWindow == nullptr) {
    SystemLogger::LogError("Failed to create main Win32 window.");
    return 1;
  }

  // Initialize Package Providers
  m_providers = PackageProviderRegistry::GetRegisteredProviders();

  // Set default manifest directory
  auto defaultRootDirectory = PathResolver::GetDefaultInstallationRootPath();
  m_manifest = std::make_unique<ManifestManager>(defaultRootDirectory);

  // Setup controls and DPI scaling
  CreateControls(m_mainWindow);
  InitializeDemosceneAssets(m_mainWindow);
  RecalculateLayoutAndDpi(m_mainWindow);

  // Display Current Installation Path
  std::wstring const displayPath = L"Installation Path: " + defaultRootDirectory.wstring();
  SetWindowTextW(m_pathDisplayEdit, displayPath.c_str());
  SetWindowTextW(m_changePathButton, L"Change Path");

  RefreshInstalledList();

  // Show window
  ShowWindow(m_mainWindow, cmdShow);
  UpdateWindow(m_mainWindow);

  // Start a timer for the demoscene scrolling ticker & starfield updates (every 25ms ~ 40FPS)
  SetTimer(m_mainWindow, 1001, 25, nullptr);

  // Log startup sequence
  AppendLog(L"CatUpdate GUI initialized.");
  AppendLog(L"Default installation root: " + defaultRootDirectory.wstring());

  // Main message dispatch loop
  MSG windowMessage;
  while (GetMessage(&windowMessage, nullptr, 0, 0)) {
    TranslateMessage(&windowMessage);
    DispatchMessage(&windowMessage);
  }

  // Cleanup GDI resources
  if (m_demosceneFont != nullptr) {
    DeleteObject(m_demosceneFont);
  }
  if (m_scrollerFont != nullptr) {
    DeleteObject(m_scrollerFont);
  }
  if (m_backgroundBrush != nullptr) {
    DeleteObject(m_backgroundBrush);
  }

  return static_cast<int>(windowMessage.wParam);
}

// -----------------------------------------------------------------------------
// Controls Creation & Custom Subclass Styling
// -----------------------------------------------------------------------------

void DesktopUserInterface::CreateControls(HWND parentWindow) {
  // Get default system GUI font
  auto defaultSystemFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

  // Software Packages ListView (Grid view of applications)
  m_packageListView =
      CreateWindowExW(0, WC_LISTVIEW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL, 20, 70, 745,
                      250, parentWindow, reinterpret_cast<HMENU>(2001), m_hinstance, nullptr);
  ListView_SetExtendedListViewStyle(m_packageListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
  SendMessage(m_packageListView, WM_SETFONT, reinterpret_cast<WPARAM>(defaultSystemFont), TRUE);

  // Apply Cyberpunk theme colors to ListView
  ListView_SetBkColor(m_packageListView, RGB(12, 12, 25));
  ListView_SetTextBkColor(m_packageListView, RGB(12, 12, 25));
  ListView_SetTextColor(m_packageListView, RGB(0, 240, 255));

  // Insert Columns
  LVCOLUMNW softwareNameColumn = {};
  softwareNameColumn.mask = LVCF_TEXT | LVCF_WIDTH;
  softwareNameColumn.pszText = const_cast<LPWSTR>(L"Software Package");
  softwareNameColumn.cx = 250;
  ListView_InsertColumn(m_packageListView, 0, &softwareNameColumn);

  LVCOLUMNW installedVersionColumn = {};
  installedVersionColumn.mask = LVCF_TEXT | LVCF_WIDTH;
  installedVersionColumn.pszText = const_cast<LPWSTR>(L"Installed Version");
  installedVersionColumn.cx = 180;
  ListView_InsertColumn(m_packageListView, 1, &installedVersionColumn);

  LVCOLUMNW packageStatusColumn = {};
  packageStatusColumn.mask = LVCF_TEXT | LVCF_WIDTH;
  packageStatusColumn.pszText = const_cast<LPWSTR>(L"Status");
  packageStatusColumn.cx = 280;
  ListView_InsertColumn(m_packageListView, 2, &packageStatusColumn);

  // Available versions dropdown ComboBox
  m_versionComboBox = CreateWindowExW(0, WC_COMBOBOX, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 20, 335, 200, 200,
                                      parentWindow, reinterpret_cast<HMENU>(2002), m_hinstance, nullptr);
  SendMessage(m_versionComboBox, WM_SETFONT, reinterpret_cast<WPARAM>(defaultSystemFont), TRUE);

  // Custom Cyberpunk Styled Buttons
  m_installButton = CreateWindowExW(0, WC_BUTTON, L"INSTALL / UPDATE", WS_CHILD | WS_VISIBLE | WS_DISABLED, 235, 335,
                                    180, 30, parentWindow, reinterpret_cast<HMENU>(2003), m_hinstance, nullptr);
  SetWindowSubclass(m_installButton, CustomButtonProc, 1, 0);

  m_changePathButton = CreateWindowExW(0, WC_BUTTON, L"CHANGE PATH", WS_CHILD | WS_VISIBLE, 430, 335, 150, 30,
                                       parentWindow, reinterpret_cast<HMENU>(2004), m_hinstance, nullptr);
  SetWindowSubclass(m_changePathButton, CustomButtonProc, 2, 0);

  m_uninstallButton = CreateWindowExW(0, WC_BUTTON, L"UNINSTALL", WS_CHILD | WS_VISIBLE | WS_DISABLED, 595, 335, 170,
                                      30, parentWindow, reinterpret_cast<HMENU>(2005), m_hinstance, nullptr);
  SetWindowSubclass(m_uninstallButton, CustomButtonProc, 3, 0);

  // Installation Path Display label
  m_pathDisplayEdit = CreateWindowExW(0, WC_EDIT, L"Installation Path: ",
                                      WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY | ES_AUTOHSCROLL | WS_BORDER, 20,
                                      375, 745, 24, parentWindow, nullptr, m_hinstance, nullptr);
  SendMessage(m_pathDisplayEdit, WM_SETFONT, reinterpret_cast<WPARAM>(defaultSystemFont), TRUE);

  // Hacker Terminal Console Log
  m_consoleLogEdit = CreateWindowExW(
      0, WC_EDIT, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 20,
      405, 745, 110, parentWindow, nullptr, m_hinstance, nullptr);

  // Status Static label
  m_statusStatusBar =
      CreateWindowExW(0, WC_STATIC, L"Select a package to begin...", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 20,
                      520, 745, 20, parentWindow, nullptr, m_hinstance, nullptr);
  SendMessage(m_statusStatusBar, WM_SETFONT, reinterpret_cast<WPARAM>(defaultSystemFont), TRUE);
}

// -----------------------------------------------------------------------------
// GDI Demoscene Particle System & Assets
// -----------------------------------------------------------------------------

void DesktopUserInterface::InitializeDemosceneAssets(HWND /*parentWindow*/) {
  // Set up the moving particle starfield (100 stars with varying parallax depths)
  m_starfield.clear();
  for (int i = 0; i < 100; ++i) {
    RetroStarParticle star{};
    star.xPosition = static_cast<float>(rand() % 800);
    star.yPosition = static_cast<float>(rand() % 610);

    // layer depth (1 is fast/bright, 3 is slow/distant)
    star.intensityDepth = (rand() % 3) + 1;

    if (star.intensityDepth == 1) {
      star.horizontalVelocity = 4.5F;
    } else if (star.intensityDepth == 2) {
      star.horizontalVelocity = 2.5F;
    } else {
      star.horizontalVelocity = 1.0F;
    }

    m_starfield.push_back(star);
  }

  // Create GDI Fonts for beautiful hacker themed text rendering
  m_demosceneFont = CreateFontW(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                                CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, L"Consolas");

  m_scrollerFont = CreateFontW(20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                               CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH, L"Courier New");
}

// -----------------------------------------------------------------------------
// High-DPI Auto-Scaling Calculation
// -----------------------------------------------------------------------------

void DesktopUserInterface::RecalculateLayoutAndDpi(HWND parentWindow) {
  HDC deviceContext = GetDC(parentWindow);
  int const pixelsPerInch = GetDeviceCaps(deviceContext, LOGPIXELSX);
  ReleaseDC(parentWindow, deviceContext);

  m_dpiScaleFactor = static_cast<float>(pixelsPerInch) / 96.0F;

  // Set Courier New monospaced font on terminal log edit
  if ((m_consoleLogEdit != nullptr) && (m_scrollerFont != nullptr)) {
    SendMessage(m_consoleLogEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_scrollerFont), TRUE);
  }

  // Scale standard static controls and columns for crisp rendering
  ListView_SetColumnWidth(m_packageListView, 0, static_cast<int>(250 * m_dpiScaleFactor));
  ListView_SetColumnWidth(m_packageListView, 1, static_cast<int>(180 * m_dpiScaleFactor));
  ListView_SetColumnWidth(m_packageListView, 2, static_cast<int>(280 * m_dpiScaleFactor));
}

// -----------------------------------------------------------------------------
// Double Buffered GDI Paint Handlers
// -----------------------------------------------------------------------------

void DesktopUserInterface::PaintDemosceneScreen(HWND hwnd, HDC hdc) {
  RECT clientRectangle;
  GetClientRect(hwnd, &clientRectangle);
  int const width = clientRectangle.right;
  int const height = clientRectangle.bottom;

  // 1. Initialize Double Buffering to completely eliminate flickers
  HDC memoryDeviceContext = CreateCompatibleDC(hdc);
  HBITMAP memoryBitmap = CreateCompatibleBitmap(hdc, width, height);
  auto oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDeviceContext, memoryBitmap));

  // 2. Draw solid dark space background
  FillRect(memoryDeviceContext, &clientRectangle, m_backgroundBrush);

  // 3. Render the particle starfield
  for (const auto& star : m_starfield) {
    COLORREF starColor = RGB(30, 30, 50); // Default dim distant star
    if (star.intensityDepth == 1) {
      starColor = RGB(57, 255, 20); // Glowing Cyber Green
    } else if (star.intensityDepth == 2) {
      starColor = RGB(0, 240, 255); // Neon Cyan
    } else {
      starColor = RGB(130, 0, 200); // Electric Purple
    }

    // Draw star pixel/small square
    for (int dx = 0; dx < 2; ++dx) {
      for (int dy = 0; dy < 2; ++dy) {
        SetPixel(memoryDeviceContext, static_cast<int>(star.xPosition) + dx, static_cast<int>(star.yPosition) + dy,
                 starColor);
      }
    }
  }

  // 4. Draw Top Hacker banner heading
  auto originalFont = static_cast<HFONT>(SelectObject(memoryDeviceContext, m_demosceneFont));
  SetBkMode(memoryDeviceContext, TRANSPARENT);

  // Draw header text with glowing shadow effect
  SetTextColor(memoryDeviceContext, RGB(120, 0, 220));
  TextOutW(memoryDeviceContext, 22, 18, L"*** CATUPDATE SOFTWARE CENTER ***", 33);
  SetTextColor(memoryDeviceContext, RGB(0, 240, 255));
  TextOutW(memoryDeviceContext, 20, 16, L"*** CATUPDATE SOFTWARE CENTER ***", 33);

  // 5. Render the sinusoidal waving text scroller at the bottom
  SelectObject(memoryDeviceContext, m_scrollerFont);
  SetTextColor(memoryDeviceContext, RGB(57, 255, 20)); // Hacker Retro Green

  int const scrollerBaseY = height - 35;
  int const startDrawX = static_cast<int>(m_tickerScrollOffset);

  for (size_t i = 0; i < m_tickerText.length(); ++i) {
    wchar_t const character = m_tickerText[i];
    int const charWidth = 11; // Approximate monospaced width
    int const xCoordinate = startDrawX + static_cast<int>(i * charWidth);

    // Only draw if within bounds
    if (xCoordinate > -20 && xCoordinate < width + 20) {
      // Sine wave calculation mapping Y vertical displacement
      float const sineValue = std::sin((static_cast<float>(xCoordinate) / 45.0F) + m_sineWavePhase);
      int const yOffset = static_cast<int>(sineValue * 15.0F);
      int const yCoordinate = scrollerBaseY + yOffset;

      TextOutW(memoryDeviceContext, xCoordinate, yCoordinate, &character, 1);
    }
  }

  // 6. Restore GDI environment and dump memory DC onto the screen
  SelectObject(memoryDeviceContext, originalFont);
  BitBlt(hdc, 0, 0, width, height, memoryDeviceContext, 0, 0, SRCCOPY);

  SelectObject(memoryDeviceContext, oldBitmap);
  DeleteObject(memoryBitmap);
  DeleteDC(memoryDeviceContext);
}

void DesktopUserInterface::UpdateDemosceneAnimation(HWND hwnd) {
  RECT clientRectangle;
  GetClientRect(hwnd, &clientRectangle);
  int const width = clientRectangle.right;

  // Scroll the scroller offset leftwards
  m_tickerScrollOffset -= 2.0F;

  // Wrap scroller text when finished
  int const charWidth = 11;
  int const textTotalWidth = static_cast<int>(m_tickerText.length() * charWidth);
  if (m_tickerScrollOffset < -textTotalWidth) {
    m_tickerScrollOffset = static_cast<float>(width);
  }

  // Shift sinusoidal wave phase factor
  m_sineWavePhase += 0.08F;

  // Shift star positions horizontally
  for (auto& star : m_starfield) {
    star.xPosition -= star.horizontalVelocity;
    if (star.xPosition < -5) {
      star.xPosition = static_cast<float>(width + 5);
      star.yPosition = static_cast<float>(rand() % 610);
    }
  }

  // Force partial redraw of animation areas without erasing background, fully avoiding flicker
  RECT const scrollerRect = {
      .left = 0, .top = clientRectangle.bottom - 65, .right = clientRectangle.right, .bottom = clientRectangle.bottom};
  InvalidateRect(hwnd, &scrollerRect, FALSE);

  RECT const headerRect = {.left = 0, .top = 0, .right = clientRectangle.right, .bottom = 65};
  InvalidateRect(hwnd, &headerRect, FALSE);

  // Repaint background regions around ListView
  RECT const backgroundLeftRect = {.left = 0, .top = 65, .right = 18, .bottom = clientRectangle.bottom - 65};
  InvalidateRect(hwnd, &backgroundLeftRect, FALSE);

  RECT const backgroundRightRect = {.left = clientRectangle.right - 18,
                                    .top = 65,
                                    .right = clientRectangle.right,
                                    .bottom = clientRectangle.bottom - 65};
  InvalidateRect(hwnd, &backgroundRightRect, FALSE);
}

// -----------------------------------------------------------------------------
// Custom Painted Subclassed Button Control Procedural
// -----------------------------------------------------------------------------

LRESULT CALLBACK DesktopUserInterface::CustomButtonProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
                                                        UINT_PTR /*subclassId*/, DWORD_PTR /*refData*/) {
  switch (message) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rect;
    GetClientRect(hwnd, &rect);
    int const width = rect.right;
    int const height = rect.bottom;

    // Draw gorgeous custom styled Cyberpunk buttons
    HBRUSH borderBrush = CreateSolidBrush(RGB(0, 240, 255));  // Neon Cyan border
    HBRUSH interiorBrush = CreateSolidBrush(RGB(12, 12, 25)); // Dark Navy background

    FillRect(hdc, &rect, borderBrush);

    // Deflate interior rect to render border outline
    RECT const interiorRect = {
        .left = rect.left + 2, .top = rect.top + 2, .right = rect.right - 2, .bottom = rect.bottom - 2};
    FillRect(hdc, &interiorRect, interiorBrush);

    // Fetch text content
    wchar_t buttonText[128];
    GetWindowTextW(hwnd, buttonText, 128);

    // Render text
    auto defaultSystemFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    auto oldFont = static_cast<HFONT>(SelectObject(hdc, defaultSystemFont));

    SetBkMode(hdc, TRANSPARENT);
    if (IsWindowEnabled(hwnd) != 0) {
      SetTextColor(hdc, RGB(57, 255, 20)); // Bright Neon Green text
    } else {
      SetTextColor(hdc, RGB(70, 70, 85)); // Dim grey text
    }

    DrawTextW(hdc, buttonText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(borderBrush);
    DeleteObject(interiorBrush);

    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_ERASEBKGND:
    return 1; // Handled
  }
  return DefSubclassProc(hwnd, message, wparam, lparam);
}

// -----------------------------------------------------------------------------
// UI Action Events (Selection, Path Changes, & Threaded Installs)
// -----------------------------------------------------------------------------

void DesktopUserInterface::RefreshInstalledList() {
  ListView_DeleteAllItems(m_packageListView);
  m_selectedPackageIndex = -1;
  EnableWindow(m_installButton, FALSE);
  EnableWindow(m_uninstallButton, FALSE);
  SendMessage(m_versionComboBox, CB_RESETCONTENT, 0, 0);

  auto installed = m_manifest->GetInstalledPackages();

  for (size_t i = 0; i < m_providers.size(); ++i) {
    LVITEMW listItem = {.mask = 0};
    listItem.mask = LVIF_TEXT;
    listItem.iItem = static_cast<int>(i);

    // Package Name
    std::wstring const displayName = Utils::ToWString(m_providers[i]->GetDisplayName());
    listItem.pszText = const_cast<LPWSTR>(displayName.c_str());
    ListView_InsertItem(m_packageListView, &listItem);

    // Installed version query
    std::wstring installedVersionStr = L"Not Installed";
    for (const auto& installedPackageState : installed) {
      if (installedPackageState.identifier == m_providers[i]->GetIdentifier()) {
        installedVersionStr = Utils::ToWString(installedPackageState.installedVersion);
        break;
      }
    }
    ListView_SetItemText(m_packageListView, static_cast<int>(i), 1, const_cast<LPWSTR>(installedVersionStr.c_str()));

    // Status
    std::wstring const statusText =
        (installedVersionStr == L"Not Installed") ? L"Available" : L"Installed & Registered";
    ListView_SetItemText(m_packageListView, static_cast<int>(i), 2, const_cast<LPWSTR>(statusText.c_str()));
  }
}

void DesktopUserInterface::OnPackageSelectionChanged() {
  int const selectedIndex = ListView_GetNextItem(m_packageListView, -1, LVNI_SELECTED);
  if (selectedIndex == -1 || selectedIndex == m_selectedPackageIndex) {
    return;
  }
  m_selectedPackageIndex = selectedIndex;

  std::wstring const packageDisplayName = Utils::ToWString(m_providers[selectedIndex]->GetDisplayName());
  AppendLog(L"Selected package: " + packageDisplayName);
  AppendLog(L"Querying remote versions registry...");

  SetWindowTextW(m_statusStatusBar, L"Status: Querying remote versions registry...");
  EnableWindow(m_installButton, FALSE);

  // Enable/disable uninstall button immediately based on local installation state
  bool const isInstalled = m_manifest->IsPackageInstalled(m_providers[selectedIndex]->GetIdentifier());
  EnableWindow(m_uninstallButton, isInstalled ? TRUE : FALSE);

  SendMessage(m_versionComboBox, CB_RESETCONTENT, 0, 0);

  // Fetch versions asynchronously to prevent GUI freezing
  std::thread([selectedIndex]() {
    auto httpClient = HttpClientFactory::CreateDefaultClient();
    auto versionsList = m_providers[selectedIndex]->FetchAvailableVersions(*httpClient);

    // Safely allocate version list on the heap to pass across threads
    auto* heapVersionsList = new std::vector<PackageVersion>(std::move(versionsList));

    // Populate UI in main thread context
    SendMessage(m_mainWindow, WM_COMMAND, MAKEWPARAM(3001, 0), reinterpret_cast<LPARAM>(heapVersionsList));
  }).detach();
}

void DesktopUserInterface::TriggerInstallation() {
  int const packageIndex = m_selectedPackageIndex;
  int const versionIndex = static_cast<int>(SendMessage(m_versionComboBox, CB_GETCURSEL, 0, 0));
  if (packageIndex == -1 || versionIndex == -1) {
    return;
  }

  wchar_t buffer[256];
  SendMessage(m_versionComboBox, CB_GETLBTEXT, versionIndex, reinterpret_cast<LPARAM>(buffer));
  std::wstring const wVer(buffer);
  std::string const version = Utils::ToString(wVer);

  SetWindowTextW(m_statusStatusBar, L"Status: Starting download sequence...");
  EnableWindow(m_installButton, FALSE);
  EnableWindow(m_uninstallButton, FALSE);

  // Download & Extract inside background thread to maintain smooth scroller FPS
  std::thread([packageIndex, version, wVer]() {
    auto* provider = m_providers[packageIndex].get();
    auto url = provider->GetDownloadUrl(version);
    auto destinationDir = m_manifest->GetInstallationRootDirectory() / provider->GetIdentifier();
    auto tempArchive = m_manifest->GetInstallationRootDirectory() / ("temp_" + provider->GetArchiveFilename(version));

    std::wstring displayName = Utils::ToWString(provider->GetDisplayName());

    // Thread-safe log reporting via window messages
    PostMessage(m_mainWindow, WM_APP + 1, 0,
                reinterpret_cast<LPARAM>(new std::wstring(L"--------------------------------------------------")));
    PostMessage(m_mainWindow, WM_APP + 1, 0,
                reinterpret_cast<LPARAM>(
                    new std::wstring(std::format(L"Installation target: {} (Version: {})", displayName, wVer))));
    PostMessage(m_mainWindow, WM_APP + 1, 0,
                reinterpret_cast<LPARAM>(new std::wstring(L"Source URL: " + Utils::ToWString(url))));

    std::filesystem::create_directories(m_manifest->GetInstallationRootDirectory());

    auto httpClient = HttpClientFactory::CreateDefaultClient();

    // Safe progress throttle tracker to avoid log buffer overflow
    struct DownloadProgressTracker {
      int lastLoggedPercent = -10;
    };
    auto tracker = std::make_shared<DownloadProgressTracker>();

    bool const downloadSuccess = httpClient->DownloadFile(url, tempArchive, [tracker](float progress) {
      int percent = static_cast<int>(progress * 100.0F);
      if (percent >= tracker->lastLoggedPercent + 10 || percent == 100) {
        tracker->lastLoggedPercent = percent / 10 * 10;
        std::wstring const progressLog = std::format(L"Binaries download progress: {}%", percent);
        PostMessage(m_mainWindow, WM_APP + 1, 0, reinterpret_cast<LPARAM>(new std::wstring(progressLog)));
      }
      std::wstring const text = std::format(L"Status: Downloading package binaries... {:.1f}%", progress * 100.0F);
      SetWindowTextW(m_statusStatusBar, text.c_str());
    });

    if (downloadSuccess) {
      PostMessage(m_mainWindow, WM_APP + 1, 0,
                  reinterpret_cast<LPARAM>(new std::wstring(L"Download completed successfully.")));
      PostMessage(m_mainWindow, WM_APP + 1, 0,
                  reinterpret_cast<LPARAM>(new std::wstring(L"Extracting files using native tar.exe...")));

      SetWindowTextW(m_statusStatusBar, L"Status: Extracting compressed archive files...");
      std::filesystem::create_directories(destinationDir);

      std::vector<std::string> const extractionCommand = {"tar.exe", "-xf", tempArchive.string(), "-C",
                                                          destinationDir.string()};

      auto extractionResult = ProcessExecutor::ExecuteCommand(extractionCommand);
      std::filesystem::remove(tempArchive); // Cleanup temp zip file

      if (extractionResult.has_value() && extractionResult->exitCode == 0) {
        InstalledPackageState state;
        state.identifier = provider->GetIdentifier();
        state.installedVersion = version;
        state.installationPath = destinationDir;

        // Fetch current formatted date
        auto now = std::chrono::system_clock::now();
        std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm timeStruct = *std::localtime(&nowTime);
        state.installationDate =
            std::format("{:04}-{:02}-{:02}", timeStruct.tm_year + 1900, timeStruct.tm_mon + 1, timeStruct.tm_mday);

        m_manifest->RegisterOrUpdateInstalledPackage(state);

        PostMessage(m_mainWindow, WM_APP + 1, 0, reinterpret_cast<LPARAM>(new std::wstring(L"Extraction complete.")));
        PostMessage(
            m_mainWindow, WM_APP + 1, 0,
            reinterpret_cast<LPARAM>(new std::wstring(std::format(
                L"Success: {} successfully installed at {}", displayName, Utils::ToWString(destinationDir.string())))));
        SetWindowTextW(m_statusStatusBar, L"Status: Installation completed and registered successfully!");
      } else {
        PostMessage(m_mainWindow, WM_APP + 1, 0,
                    reinterpret_cast<LPARAM>(
                        new std::wstring(L"ERROR: Archive extraction failed (tar.exe exit code non-zero).")));
        SetWindowTextW(m_statusStatusBar, L"Status: Extraction failed. Archive file might be corrupted.");
      }
    } else {
      PostMessage(m_mainWindow, WM_APP + 1, 0,
                  reinterpret_cast<LPARAM>(new std::wstring(L"ERROR: Download request failed.")));
      SetWindowTextW(m_statusStatusBar, L"Status: Network error. Binaries download failed.");
    }

    // Reset controls state
    EnableWindow(m_installButton, TRUE);
    SendMessage(m_mainWindow, WM_COMMAND, MAKEWPARAM(3002, 0), 0); // Trigger list refresh
  }).detach();
}

void DesktopUserInterface::TriggerPathChange() {
  BROWSEINFOW folderBrowseInfo = {.hwndOwner = nullptr};
  folderBrowseInfo.hwndOwner = m_mainWindow;
  folderBrowseInfo.lpszTitle = L"Select No-Admin Installation Directory";
  folderBrowseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  LPITEMIDLIST pidl = SHBrowseForFolderW(&folderBrowseInfo);
  if (pidl != nullptr) {
    wchar_t selectedPath[MAX_PATH];
    if (SHGetPathFromIDListW(pidl, selectedPath) != 0) {
      std::filesystem::path const path(selectedPath);

      // Re-instantiate Manifest at new path
      m_manifest = std::make_unique<ManifestManager>(path);

      std::wstring const displayPath = L"Installation Path: " + path.wstring();
      SetWindowTextW(m_pathDisplayEdit, displayPath.c_str());

      AppendLog(L"Installation root path changed to: " + path.wstring());

      RefreshInstalledList();
      SetWindowTextW(m_statusStatusBar, L"Status: Target installation directory changed.");
    }
    CoTaskMemFree(pidl);
  }
}

void DesktopUserInterface::TriggerUninstallation() {
  int const packageIndex = m_selectedPackageIndex;
  if (packageIndex == -1) {
    return;
  }

  auto* provider = m_providers[packageIndex].get();
  auto packageId = provider->GetIdentifier();
  std::wstring const displayName = Utils::ToWString(provider->GetDisplayName());

  // Disable buttons during action
  EnableWindow(m_installButton, FALSE);
  EnableWindow(m_uninstallButton, FALSE);
  SetWindowTextW(m_statusStatusBar, L"Status: Starting uninstall sequence...");

  // Run uninstallation on background thread
  std::thread([packageIndex, packageId, displayName]() {
    PostMessage(m_mainWindow, WM_APP + 1, 0,
                reinterpret_cast<LPARAM>(new std::wstring(L"--------------------------------------------------")));
    PostMessage(m_mainWindow, WM_APP + 1, 0,
                reinterpret_cast<LPARAM>(new std::wstring(std::format(L"Uninstallation target: {}", displayName))));

    auto packageState = m_manifest->GetInstalledPackageByIdentifier(packageId);
    if (!packageState.has_value()) {
      PostMessage(m_mainWindow, WM_APP + 1, 0,
                  reinterpret_cast<LPARAM>(new std::wstring(L"ERROR: Package is not registered as installed.")));
      SetWindowTextW(m_statusStatusBar, L"Status: Uninstallation failed.");

      // Re-enable controls via message 3002
      SendMessage(m_mainWindow, WM_COMMAND, MAKEWPARAM(3002, 0), 0);
      return;
    }

    std::wstring const installPathStr = Utils::ToWString(packageState->installationPath.string());
    PostMessage(m_mainWindow, WM_APP + 1, 0,
                reinterpret_cast<LPARAM>(new std::wstring(L"Removing installation files at: " + installPathStr)));

    try {
      if (std::filesystem::exists(packageState->installationPath)) {
        std::filesystem::remove_all(packageState->installationPath);
      }

      m_manifest->UnregisterInstalledPackage(packageId);

      PostMessage(m_mainWindow, WM_APP + 1, 0,
                  reinterpret_cast<LPARAM>(new std::wstring(L"Files removed successfully.")));
      PostMessage(m_mainWindow, WM_APP + 1, 0,
                  reinterpret_cast<LPARAM>(
                      new std::wstring(std::format(L"Success: {} uninstalled successfully.", displayName))));
      SetWindowTextW(m_statusStatusBar, L"Status: Uninstallation completed successfully!");
    } catch (const std::exception& ex) {
      std::wstring const errorMsg = Utils::ToWString(ex.what());
      PostMessage(m_mainWindow, WM_APP + 1, 0,
                  reinterpret_cast<LPARAM>(new std::wstring(L"ERROR: Failed to uninstall package: " + errorMsg)));
      SetWindowTextW(m_statusStatusBar, L"Status: Uninstallation failed.");
    }

    // Trigger refresh and reset buttons on the main thread
    SendMessage(m_mainWindow, WM_COMMAND, MAKEWPARAM(3002, 0), 0);
  }).detach();
}

// -----------------------------------------------------------------------------
// Main WndProc Callbacks
// -----------------------------------------------------------------------------

LRESULT CALLBACK DesktopUserInterface::MainWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
  case WM_CREATE:
    return 0;
  case WM_NCHITTEST: {
    LRESULT const hit = DefWindowProcW(hwnd, message, wparam, lparam);
    if (hit == HTCLIENT) {
      return HTCAPTION;
    }
    return hit;
  }
  case WM_TIMER:
    if (wparam == 1001) {
      UpdateDemosceneAnimation(hwnd);
    }
    return 0;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    PaintDemosceneScreen(hwnd, hdc);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORSTATIC: {
    HDC hdcStatic = reinterpret_cast<HDC>(wparam);
    HWND hwndControl = reinterpret_cast<HWND>(lparam);

    // Hacker console log is green, status/paths are neon cyan
    if (hwndControl == m_consoleLogEdit) {
      SetTextColor(hdcStatic, RGB(57, 255, 20));
    } else {
      SetTextColor(hdcStatic, RGB(0, 240, 255));
    }

    SetBkMode(hdcStatic, TRANSPARENT);
    return reinterpret_cast<LRESULT>(m_backgroundBrush);
  }
  case WM_ERASEBKGND:
    return 1; // Double buffer handles erasing entirely to avoid flicker
  case WM_NOTIFY: {
    auto const* notifyHeader = reinterpret_cast<NMHDR*>(lparam);
    if (notifyHeader->idFrom == 2001 && notifyHeader->code == LVN_ITEMCHANGED) {
      OnPackageSelectionChanged();
    }
    break;
  }
  case WM_COMMAND: {
    int const commandId = LOWORD(wparam);
    int const notificationCode = HIWORD(wparam);

    if (commandId == 2003) {
      TriggerInstallation();
    } else if (commandId == 2004) {
      TriggerPathChange();
    } else if (commandId == 2005) {
      TriggerUninstallation();
    }

    // Threading Messages
    if (commandId == 3001) { // Asynchronous versions query callback completed
      // Wrap in std::unique_ptr to ensure the heap-allocated vector is cleaned up automatically
      std::unique_ptr<std::vector<PackageVersion>> const versions(
          reinterpret_cast<std::vector<PackageVersion>*>(lparam));
      for (const auto& v : *versions) {
        std::wstring const wVersion = Utils::ToWString(v);
        SendMessage(m_versionComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wVersion.c_str()));
      }
      SendMessage(m_versionComboBox, CB_SETCURSEL, 0, 0);
      EnableWindow(m_installButton, TRUE);
      SetWindowTextW(m_statusStatusBar, L"Status: Versions registry updated.");
      AppendLog(L"Loaded available versions list successfully.");
    } else if (commandId == 3002) { // Refresh package listings
      RefreshInstalledList();
    }
    break;
  }
  case WM_APP + 1: { // Asynchronous Thread Safe Logging message
    std::unique_ptr<std::wstring> const logMessage(reinterpret_cast<std::wstring*>(lparam));
    AppendLog(*logMessage);
    return 0;
  }
  case WM_DESTROY:
    KillTimer(hwnd, 1001);
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

void DesktopUserInterface::AppendLog(const std::wstring& message) {
  if (m_consoleLogEdit == nullptr) {
    return;
  }

  // Get current local system time
  auto now = std::chrono::system_clock::now();
  std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  std::tm timeStruct = *std::localtime(&nowTime);

  std::wstring const timeString =
      std::format(L"[{:02}:{:02}:{:02}] ", timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec);

  std::wstring const fullLine = timeString + message + L"\r\n";

  // Set selection focus to the end of edit control and append text
  int const length = GetWindowTextLengthW(m_consoleLogEdit);
  SendMessage(m_consoleLogEdit, EM_SETSEL, length, length);
  SendMessage(m_consoleLogEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(fullLine.c_str()));
}

} // namespace CatUpdate

// Win32 Entry point
extern "C" int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nCmdShow) {
  return CatUpdate::DesktopUserInterface::Run(hInstance, nCmdShow);
}

#endif // _WIN32
