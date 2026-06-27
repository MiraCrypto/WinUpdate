#include "CatUpdateCore.hpp"
#include "PackageManager.hpp"
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
#include <shobjidl.h>

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

  // Initialize COM for modern Explorer folder picker
  HRESULT const comInitResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(comInitResult)) {
    SystemLogger::LogError("Failed to initialize COM library.");
    return 1;
  }

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
  m_darkComboBoxBrush = CreateSolidBrush(RGB(12, 12, 25));
  windowClass.hbrBackground = m_backgroundBrush;
  windowClass.lpszClassName = L"CatUpdateMainWindowClass";

  if (RegisterClassExW(&windowClass) == 0U) {
    SystemLogger::LogError("Failed to register main Win32 window class.");
    return 1;
  }

  // Create the Main Window
  m_mainWindow = CreateWindowExW(WS_EX_APPWINDOW, L"CatUpdateMainWindowClass", L"Software Center - No-Admin",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
                                 800, 650, nullptr, nullptr, hinstance, nullptr);

  if (m_mainWindow == nullptr) {
    SystemLogger::LogError("Failed to create main Win32 window.");
    return 1;
  }

  // Initialize Package Providers (filter for Windows support at runtime)
  auto allProviders = PackageProviderRegistry::GetRegisteredProviders();
  for (auto& provider : allProviders) {
    if (provider->IsPlatformSupported(PlatformType::Windows, PlatformTraits::GetHostArchitecture())) {
      m_providers.push_back(std::move(provider));
    }
  }

  // Set default manifest directory
  auto defaultRootDirectory = PathResolver::GetDefaultInstallationRootPath();
  m_manifest = std::make_unique<ManifestManager>(defaultRootDirectory);
  m_packageManager = std::make_unique<PackageManager>(*m_manifest);

  // Setup controls and DPI scaling
  CreateControls(m_mainWindow);
  InitializeDemosceneAssets(m_mainWindow);
  RecalculateLayoutAndDpi(m_mainWindow);

  // Display Current Installation Path options
  PopulatePathComboBox();

  RefreshInstalledList();

  // Show window
  ShowWindow(m_mainWindow, cmdShow);
  UpdateWindow(m_mainWindow);

  // Start a timer for the demoscene scrolling ticker & starfield updates (every 25ms ~ 40FPS)
  SetTimer(m_mainWindow, 1001, 25, nullptr);

  // Log startup sequence
  AppendLog(L"Software Center GUI initialized.");
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
  if (m_darkComboBoxBrush != nullptr) {
    DeleteObject(m_darkComboBoxBrush);
  }

  CoUninitialize();
  return static_cast<int>(windowMessage.wParam);
}

// -----------------------------------------------------------------------------
// Controls Creation & Custom Subclass Styling
// -----------------------------------------------------------------------------

void DesktopUserInterface::CreateControls(HWND parentWindow) {
  // Get default system GUI font
  auto* defaultSystemFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

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
                                    250, 30, parentWindow, reinterpret_cast<HMENU>(2003), m_hinstance, nullptr);
  SetWindowSubclass(m_installButton, CustomButtonProc, 1, 0);

  m_uninstallButton = CreateWindowExW(0, WC_BUTTON, L"UNINSTALL", WS_CHILD | WS_VISIBLE | WS_DISABLED, 500, 335, 265,
                                      30, parentWindow, reinterpret_cast<HMENU>(2005), m_hinstance, nullptr);
  SetWindowSubclass(m_uninstallButton, CustomButtonProc, 3, 0);

  // Path Selection ComboBox
  m_pathComboBox = CreateWindowExW(0, WC_COMBOBOX, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 20, 375, 590, 200,
                                   parentWindow, reinterpret_cast<HMENU>(2007), m_hinstance, nullptr);
  SendMessage(m_pathComboBox, WM_SETFONT, reinterpret_cast<WPARAM>(defaultSystemFont), TRUE);

  // Browse Path Button next to the ComboBox
  m_changePathButton = CreateWindowExW(0, WC_BUTTON, L"BROWSE...", WS_CHILD | WS_VISIBLE, 620, 375, 145, 26,
                                       parentWindow, reinterpret_cast<HMENU>(2004), m_hinstance, nullptr);
  SetWindowSubclass(m_changePathButton, CustomButtonProc, 2, 0);

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
                               CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH, L"Consolas");
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
  auto* oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDeviceContext, memoryBitmap));

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
  auto* originalFont = static_cast<HFONT>(SelectObject(memoryDeviceContext, m_demosceneFont));
  SetBkMode(memoryDeviceContext, TRANSPARENT);

  // Draw header text with glowing shadow effect
  SetTextColor(memoryDeviceContext, RGB(120, 0, 220));
  TextOutW(memoryDeviceContext, 22, 18, L"*** SOFTWARE CENTER ***", 23);
  SetTextColor(memoryDeviceContext, RGB(0, 240, 255));
  TextOutW(memoryDeviceContext, 20, 16, L"*** SOFTWARE CENTER ***", 23);

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
    HBRUSH borderBrush = CreateSolidBrush(RGB(0, 240, 255)); // Neon Cyan border
    HBRUSH interiorBrush = nullptr;

    bool const isPressed = (SendMessage(hwnd, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;
    if (isPressed) {
      interiorBrush = CreateSolidBrush(RGB(0, 240, 255)); // Invert to full Neon Cyan when pressed
    } else {
      interiorBrush = CreateSolidBrush(RGB(12, 12, 25)); // Dark Navy background
    }

    FillRect(hdc, &rect, borderBrush);

    // Deflate interior rect to render border outline
    RECT const interiorRect = {
        .left = rect.left + 2, .top = rect.top + 2, .right = rect.right - 2, .bottom = rect.bottom - 2};
    FillRect(hdc, &interiorRect, interiorBrush);

    // Fetch text content
    wchar_t buttonText[128];
    GetWindowTextW(hwnd, buttonText, 128);

    // Render text
    auto* defaultSystemFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    auto* oldFont = static_cast<HFONT>(SelectObject(hdc, defaultSystemFont));

    SetBkMode(hdc, TRANSPARENT);
    if (IsWindowEnabled(hwnd) != 0) {
      if (isPressed) {
        SetTextColor(hdc, RGB(12, 12, 25)); // Dark Navy text on bright Neon Cyan background
      } else {
        SetTextColor(hdc, RGB(57, 255, 20)); // Bright Neon Green text
      }
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
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
  case WM_MOUSEMOVE: {
    // Force immediate redraw when button state changes for responsive hover/click animations
    LRESULT const result = DefSubclassProc(hwnd, message, wparam, lparam);
    InvalidateRect(hwnd, nullptr, TRUE);
    return result;
  }
  case WM_ERASEBKGND:
    return 1; // Handled
  case WM_ENABLE:
  case WM_SETFOCUS:
  case WM_KILLFOCUS:
    InvalidateRect(hwnd, nullptr, TRUE);
    break;
  }
  return DefSubclassProc(hwnd, message, wparam, lparam);
}

// -----------------------------------------------------------------------------
// UI Action Events (Selection, Path Changes, & Threaded Installs)
// -----------------------------------------------------------------------------

void DesktopUserInterface::RefreshInstalledList() {
  m_isExecutingBackgroundAction = false;
  EnableWindow(m_changePathButton, TRUE);

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
    std::wstring statusText = L"Available";
    if (installedVersionStr != L"Not Installed") {
      std::string latestVersion;
      auto const status = m_packageManager->GetUpdateStatus(*m_providers[i], latestVersion, [i](const auto&) {
        PostMessage(m_mainWindow, WM_COMMAND, MAKEWPARAM(3001, i), 0);
      });

      if (status == PackageManager::UpdateStatus::Checking) {
        statusText = L"Checking for updates...";
      } else if (status == PackageManager::UpdateStatus::UpdateAvailable) {
        statusText = L"Update Available (v" + Utils::ToWString(latestVersion) + L")";
      } else if (status == PackageManager::UpdateStatus::UpToDate) {
        statusText = L"Up to date";
      } else if (status == PackageManager::UpdateStatus::CheckFailed) {
        statusText = L"Installed (Update check failed)";
      } else {
        statusText = L"Installed & Registered";
      }
    }
    ListView_SetItemText(m_packageListView, static_cast<int>(i), 2, const_cast<LPWSTR>(statusText.c_str()));
  }
}

void DesktopUserInterface::OnPackageSelectionChanged() {
  if (m_isExecutingBackgroundAction) {
    return;
  }
  int const selectedIndex = ListView_GetNextItem(m_packageListView, -1, LVNI_SELECTED);
  if (selectedIndex == -1 || selectedIndex == m_selectedPackageIndex) {
    return;
  }
  m_selectedPackageIndex = selectedIndex;

  auto* provider = m_providers[selectedIndex].get();
  auto const packageId = provider->GetIdentifier();
  std::wstring const packageDisplayName = Utils::ToWString(provider->GetDisplayName());

  AppendLog(L"Selected package: " + packageDisplayName);

  // Enable/disable uninstall button immediately based on local installation state
  bool const isInstalled = m_manifest->IsPackageInstalled(packageId);
  EnableWindow(m_uninstallButton, isInstalled ? TRUE : FALSE);

  SendMessage(m_versionComboBox, CB_RESETCONTENT, 0, 0);

  std::string latestVersion;
  auto const status = m_packageManager->GetUpdateStatus(*provider, latestVersion, [selectedIndex](const auto&) {
    PostMessage(m_mainWindow, WM_COMMAND, MAKEWPARAM(3001, selectedIndex), 0);
  });

  if (status == PackageManager::UpdateStatus::Checking) {
    AppendLog(L"Version query is in progress for this package.");
    SetWindowTextW(m_statusStatusBar, L"Status: Querying remote versions registry (in progress)...");
    EnableWindow(m_installButton, FALSE);
  } else if (status == PackageManager::UpdateStatus::CheckFailed) {
    SetWindowTextW(m_statusStatusBar, L"Status: Failed to retrieve versions from remote registry.");
    EnableWindow(m_installButton, FALSE);
  } else {
    auto const versions = m_packageManager->GetCachedVersions(packageId);
    if (!versions.empty()) {
      AppendLog(L"Loaded available versions list from memory cache.");
      for (const auto& version : versions) {
        std::wstring const wVersion = Utils::ToWString(version);
        SendMessage(m_versionComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wVersion.c_str()));
      }
      SendMessage(m_versionComboBox, CB_SETCURSEL, 0, 0);

      if (!m_isExecutingBackgroundAction) {
        EnableWindow(m_installButton, TRUE);
        SetWindowTextW(m_statusStatusBar, L"Status: Versions registry loaded from cache.");
      } else {
        SetWindowTextW(m_statusStatusBar, L"Status: Busy... Background action in progress.");
      }
    }
  }
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

  m_isExecutingBackgroundAction = true;
  SetWindowTextW(m_statusStatusBar, L"Status: Starting download sequence...");
  EnableWindow(m_installButton, FALSE);
  EnableWindow(m_uninstallButton, FALSE);
  EnableWindow(m_changePathButton, FALSE);

  // Execute installation workflow in background thread using core PackageManager
  std::thread([packageIndex, version]() {
    auto* provider = m_providers[packageIndex].get();
    PackageManager packageManager(*m_manifest);

    // Safe progress throttle tracker to avoid log buffer overflow
    struct DownloadProgressTracker {
      int lastLoggedPercent = -10;
    };
    auto tracker = std::make_shared<DownloadProgressTracker>();

    bool const installSuccess = packageManager.InstallPackage(
        *provider, version, PlatformType::Windows, PlatformTraits::GetHostArchitecture(),
        // Progress Callback:
        [tracker](float progress) {
          int percent = static_cast<int>(progress * 100.0F);
          if (percent >= tracker->lastLoggedPercent + 10 || percent == 100) {
            tracker->lastLoggedPercent = percent / 10 * 10;
            std::wstring const progressLog = std::format(L"Binaries download progress: {}%", percent);
            PostMessage(m_mainWindow, WM_APP + 1, 0, reinterpret_cast<LPARAM>(new std::wstring(progressLog)));
          }
          std::wstring const text = std::format(L"Status: Downloading package binaries... {:.1f}%", progress * 100.0F);
          SetWindowTextW(m_statusStatusBar, text.c_str());
        },
        // Log Callback:
        [](const std::string& message) {
          PostMessage(m_mainWindow, WM_APP + 1, 0,
                      reinterpret_cast<LPARAM>(new std::wstring(Utils::ToWString(message))));
        });

    if (installSuccess) {
      SetWindowTextW(m_statusStatusBar, L"Status: Installation completed and registered successfully!");
    } else {
      SetWindowTextW(m_statusStatusBar, L"Status: Installation/Extraction failed.");
    }

    // Reset controls state and trigger list refresh
    EnableWindow(m_installButton, TRUE);
    SendMessage(m_mainWindow, WM_COMMAND, MAKEWPARAM(3002, 0), 0);
  }).detach();
}

std::optional<std::filesystem::path> DesktopUserInterface::TriggerPathChange() {
  std::optional<std::filesystem::path> chosenPath = std::nullopt;
  IFileOpenDialog* fileOpenDialog = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fileOpenDialog));

  if (SUCCEEDED(hr)) {
    DWORD options;
    if (SUCCEEDED(fileOpenDialog->GetOptions(&options))) {
      fileOpenDialog->SetOptions(options | FOS_PICKFOLDERS);
    }

    fileOpenDialog->SetTitle(L"Select No-Admin Installation Directory");

    // Pre-set the dialog to open at the current installation path
    std::filesystem::path currentPath = m_manifest->GetInstallationRootDirectory();

    std::wstring const nativePath = currentPath.make_preferred().wstring();
    IShellItem* defaultFolderItem = nullptr;
    if (SUCCEEDED(SHCreateItemFromParsingName(nativePath.c_str(), nullptr, IID_PPV_ARGS(&defaultFolderItem)))) {
      fileOpenDialog->SetFolder(defaultFolderItem);
      defaultFolderItem->Release();
    }

    if (SUCCEEDED(fileOpenDialog->Show(m_mainWindow))) {
      IShellItem* resultItem = nullptr;
      if (SUCCEEDED(fileOpenDialog->GetResult(&resultItem))) {
        PWSTR folderPath = nullptr;
        if (SUCCEEDED(resultItem->GetDisplayName(SIGDN_FILESYSPATH, &folderPath))) {
          chosenPath = std::filesystem::path(folderPath);
          CoTaskMemFree(folderPath);
        }
        resultItem->Release();
      }
    }
    fileOpenDialog->Release();
  }
  return chosenPath;
}

void DesktopUserInterface::TriggerUninstallation() {
  int const packageIndex = m_selectedPackageIndex;
  if (packageIndex == -1) {
    return;
  }

  auto* provider = m_providers[packageIndex].get();
  auto const packageId = provider->GetIdentifier();

  // Disable buttons during action
  m_isExecutingBackgroundAction = true;
  EnableWindow(m_installButton, FALSE);
  EnableWindow(m_uninstallButton, FALSE);
  EnableWindow(m_changePathButton, FALSE);
  SetWindowTextW(m_statusStatusBar, L"Status: Starting uninstall sequence...");

  // Run uninstallation on background thread using core PackageManager
  std::thread([packageId]() {
    PackageManager packageManager(*m_manifest);
    bool const uninstallSuccess = packageManager.UninstallPackage(
        packageId,
        // Log Callback:
        [](const std::string& message) {
          PostMessage(m_mainWindow, WM_APP + 1, 0,
                      reinterpret_cast<LPARAM>(new std::wstring(Utils::ToWString(message))));
        });

    if (uninstallSuccess) {
      SetWindowTextW(m_statusStatusBar, L"Status: Uninstallation completed successfully!");
    } else {
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
  case WM_CTLCOLORLISTBOX: {
    HDC hdcList = reinterpret_cast<HDC>(wparam);
    SetTextColor(hdcList, RGB(0, 240, 255)); // Neon Cyan text
    SetBkMode(hdcList, TRANSPARENT);
    return reinterpret_cast<LRESULT>(m_darkComboBoxBrush);
  }
  case WM_ERASEBKGND:
    return 1; // Double buffer handles erasing entirely to avoid flicker
  case WM_NOTIFY: {
    auto const* notifyHeader = reinterpret_cast<NMHDR*>(lparam);
    if (notifyHeader->idFrom == 2001 && notifyHeader->code == LVN_ITEMCHANGED) {
      OnPackageSelectionChanged();
    }
    // ListView Header Custom Draw to completely eliminate the legacy grey Win98 header bar!
    if (notifyHeader->code == NM_CUSTOMDRAW) {
      auto* customDraw = reinterpret_cast<LPNMCUSTOMDRAW>(lparam);
      HWND const headerHwnd = ListView_GetHeader(m_packageListView);
      if (customDraw->hdr.hwndFrom == headerHwnd) {
        switch (customDraw->dwDrawStage) {
        case CDDS_PREPAINT:
          return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: {
          HDC const hdc = customDraw->hdc;
          RECT const rect = customDraw->rc;

          // Draw dark navy background
          HBRUSH bgBrush = CreateSolidBrush(RGB(12, 12, 25));
          FillRect(hdc, &rect, bgBrush);
          DeleteObject(bgBrush);

          // Draw a thin neon cyan border at the bottom
          HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(0, 240, 255));
          auto* oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
          MoveToEx(hdc, rect.left, rect.bottom - 1, nullptr);
          LineTo(hdc, rect.right, rect.bottom - 1);

          // Draw column vertical divider on the right
          MoveToEx(hdc, rect.right - 1, rect.top, nullptr);
          LineTo(hdc, rect.right - 1, rect.bottom);

          SelectObject(hdc, oldPen);
          DeleteObject(borderPen);

          // Fetch column text
          int const columnIndex = static_cast<int>(customDraw->dwItemSpec);
          WCHAR headerText[128] = L"";
          HDITEMW headerItem = {};
          headerItem.mask = HDI_TEXT;
          headerItem.pszText = headerText;
          headerItem.cchTextMax = 128;
          Header_GetItem(headerHwnd, columnIndex, &headerItem);

          // Draw text in Neon Cyan
          auto* defaultSystemFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
          auto* oldFont = static_cast<HFONT>(SelectObject(hdc, defaultSystemFont));
          SetTextColor(hdc, RGB(0, 240, 255));
          SetBkMode(hdc, TRANSPARENT);

          RECT textRect = rect;
          textRect.left += 6; // Padding
          DrawTextW(hdc, headerText, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

          SelectObject(hdc, oldFont);
          return CDRF_SKIPDEFAULT; // Avoid standard Windows theme painting!
        }
        }
      }
    }
    break;
  }
  case WM_COMMAND: {
    int const commandId = LOWORD(wparam);
    int const notificationCode = HIWORD(wparam);

    if (commandId == 2003) {
      TriggerInstallation();
    } else if (commandId == 2004) {
      auto const pathOpt = TriggerPathChange();
      if (pathOpt.has_value()) {
        UpdateInstallationPath(pathOpt.value());
        PopulatePathComboBox();
      }
    } else if (commandId == 2005) {
      TriggerUninstallation();
    } else if (commandId == 2007 && notificationCode == CBN_SELCHANGE) {
      OnPathSelectionChanged();
    }

    // Threading Messages
    if (commandId == 3001) { // Asynchronous versions query callback completed
      int const responsePackageIndex = static_cast<int>(HIWORD(wparam));
      auto* provider = m_providers[responsePackageIndex].get();
      auto const packageId = provider->GetIdentifier();

      // Get the updated status and latest version from the package manager
      std::string latestVersion;
      auto const status = m_packageManager->GetUpdateStatus(*provider, latestVersion);

      // 1. Update status column in ListView
      std::wstring statusText = L"Available";
      if (status == PackageManager::UpdateStatus::Checking) {
        statusText = L"Checking for updates...";
      } else if (status == PackageManager::UpdateStatus::UpdateAvailable) {
        statusText = L"Update Available (v" + Utils::ToWString(latestVersion) + L")";
      } else if (status == PackageManager::UpdateStatus::UpToDate) {
        statusText = L"Up to date";
      } else if (status == PackageManager::UpdateStatus::CheckFailed) {
        statusText = L"Installed (Update check failed)";
      } else if (status == PackageManager::UpdateStatus::NotInstalled) {
        statusText = L"Available";
      }
      ListView_SetItemText(m_packageListView, responsePackageIndex, 2, const_cast<LPWSTR>(statusText.c_str()));

      // 2. If it is still the currently selected package, update the UI
      if (responsePackageIndex == m_selectedPackageIndex) {
        SendMessage(m_versionComboBox, CB_RESETCONTENT, 0, 0);

        auto const versions = m_packageManager->GetCachedVersions(packageId);
        if (!versions.empty()) {
          for (const auto& v : versions) {
            std::wstring const wVersion = Utils::ToWString(v);
            SendMessage(m_versionComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wVersion.c_str()));
          }
          SendMessage(m_versionComboBox, CB_SETCURSEL, 0, 0);

          if (!m_isExecutingBackgroundAction) {
            EnableWindow(m_installButton, TRUE);
            SetWindowTextW(m_statusStatusBar, L"Status: Versions registry updated.");
          }
          AppendLog(L"Loaded available versions list successfully.");
        } else {
          if (!m_isExecutingBackgroundAction) {
            EnableWindow(m_installButton, FALSE);
            SetWindowTextW(m_statusStatusBar, L"Status: Failed to retrieve versions from remote registry.");
          }
          AppendLog(L"Failed to retrieve available versions list.");
        }
      } else {
        AppendLog(L"Cached background versions list for " + Utils::ToWString(provider->GetDisplayName()) + L".");
      }
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

void DesktopUserInterface::PopulatePathComboBox() {
  SendMessage(m_pathComboBox, CB_RESETCONTENT, 0, 0);

  m_predefinedPaths.clear();
  m_predefinedPaths.push_back(PathResolver::GetDefaultInstallationRootPath());
  m_predefinedPaths.push_back(PathResolver::GetLocalAppDirectory() / "SoftwareCenter");
  m_predefinedPaths.push_back(PathResolver::GetUserDocumentsDirectory());

  std::filesystem::path const currentPath = m_manifest->GetInstallationRootDirectory();
  int selectedIndex = -1;

  for (int i = 0; i < static_cast<int>(m_predefinedPaths.size()); ++i) {
    try {
      if (m_predefinedPaths[i] == currentPath ||
          (std::filesystem::exists(m_predefinedPaths[i]) && std::filesystem::exists(currentPath) &&
           std::filesystem::equivalent(m_predefinedPaths[i], currentPath))) {
        selectedIndex = i;
        break;
      }
    } catch (...) {
      if (m_predefinedPaths[i] == currentPath) {
        selectedIndex = i;
        break;
      }
    }
  }

  if (selectedIndex == -1) {
    m_predefinedPaths.push_back(currentPath);
    selectedIndex = static_cast<int>(m_predefinedPaths.size()) - 1;
  }

  std::wstring itemText;
  itemText = L"Public Documents (" + m_predefinedPaths[0].wstring() + L")";
  SendMessage(m_pathComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(itemText.c_str()));

  itemText = L"Local AppData (" + m_predefinedPaths[1].wstring() + L")";
  SendMessage(m_pathComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(itemText.c_str()));

  itemText = L"Documents (" + m_predefinedPaths[2].wstring() + L")";
  SendMessage(m_pathComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(itemText.c_str()));

  if (m_predefinedPaths.size() > 3) {
    itemText = L"Custom Path (" + m_predefinedPaths[3].wstring() + L")";
    SendMessage(m_pathComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(itemText.c_str()));
  }

  SendMessage(m_pathComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"[Browse Custom Path...]"));
  SendMessage(m_pathComboBox, CB_SETCURSEL, selectedIndex, 0);
  m_previousPathSelectedIndex = selectedIndex;
}

void DesktopUserInterface::UpdateInstallationPath(const std::filesystem::path& newPath) {
  m_manifest = std::make_unique<ManifestManager>(newPath);
  m_manifest->SaveManifestToFile();
  m_packageManager = std::make_unique<PackageManager>(*m_manifest);

  AppendLog(L"Installation root path changed to: " + newPath.wstring());

  RefreshInstalledList();

  m_selectedPackageIndex = -1;
  SendMessage(m_versionComboBox, CB_RESETCONTENT, 0, 0);
  EnableWindow(m_installButton, FALSE);
  EnableWindow(m_uninstallButton, FALSE);

  SetWindowTextW(m_statusStatusBar, L"Status: Target installation directory changed.");
}

void DesktopUserInterface::OnPathSelectionChanged() {
  int const selectedIndex = static_cast<int>(SendMessage(m_pathComboBox, CB_GETCURSEL, 0, 0));
  if (selectedIndex == CB_ERR) {
    return;
  }

  int const browseIndex = static_cast<int>(m_predefinedPaths.size());

  if (selectedIndex == browseIndex) {
    auto const pathOpt = TriggerPathChange();
    if (pathOpt.has_value()) {
      UpdateInstallationPath(pathOpt.value());
    }
    PopulatePathComboBox();
  } else if (selectedIndex >= 0 && selectedIndex < static_cast<int>(m_predefinedPaths.size())) {
    UpdateInstallationPath(m_predefinedPaths[selectedIndex]);
    m_previousPathSelectedIndex = selectedIndex;
  }
}

} // namespace CatUpdate

// Win32 Entry point
extern "C" int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nCmdShow) {
  return CatUpdate::DesktopUserInterface::Run(hInstance, nCmdShow);
}

#endif // _WIN32
