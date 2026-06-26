#pragma once

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include "CatUpdateCore.hpp"
#include "Providers.hpp"
#include <commctrl.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <windows.h>

namespace CatUpdate {

/**
 * Struct representing a particle/star in the demoscene background.
 */
struct RetroStarParticle {
  float xPosition;
  float yPosition;
  float horizontalVelocity;
  int intensityDepth; // Affects brightness and parallax layering
};

/**
 * Manages the Win32 GUI, including styling, owner-draw widgets, high-DPI scale calculations,
 * and smooth, non-blocking background thread animations.
 */
class DesktopUserInterface {
public:
  static int Run(HINSTANCE hinstance, int cmdShow);

private:
  // Window Procedure Callbacks
  static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK CustomButtonProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR subclassId,
                                           DWORD_PTR refData);

  // Initialization routines
  static void CreateControls(HWND parentWindow);
  static void RecalculateLayoutAndDpi(HWND parentWindow);
  static void InitializeDemosceneAssets(HWND parentWindow);

  // UI thread safety callbacks & event handlers
  static void OnPackageSelectionChanged();
  static void OnPathSelectionChanged();
  static void TriggerInstallation();
  static void TriggerUninstallation();
  static std::optional<std::filesystem::path> TriggerPathChange();
  static void PopulatePathComboBox();
  static void UpdateInstallationPath(const std::filesystem::path& newPath);
  static void RefreshInstalledList();
  static void AppendLog(const std::wstring& message);

  // Painting & Demoscene animations
  static void PaintDemosceneScreen(HWND hwnd, HDC hdc);
  static void UpdateDemosceneAnimation(HWND hwnd);
  static void DrawGlowingText(HDC hdc, const std::wstring& text, int x, int y, COLORREF color, int fontSize);

  // Core State
  inline static HINSTANCE m_hinstance = nullptr;
  inline static HWND m_mainWindow = nullptr;
  inline static HWND m_packageListView = nullptr;
  inline static HWND m_versionComboBox = nullptr;
  inline static HWND m_installButton = nullptr;
  inline static HWND m_uninstallButton = nullptr;
  inline static HWND m_changePathButton = nullptr;
  inline static HWND m_statusStatusBar = nullptr;
  inline static HWND m_pathComboBox = nullptr;
  inline static std::vector<std::filesystem::path> m_predefinedPaths;
  inline static int m_previousPathSelectedIndex = 0;
  inline static HWND m_consoleLogEdit = nullptr;

  inline static std::unique_ptr<ManifestManager> m_manifest;
  inline static std::vector<std::unique_ptr<PackageProvider>> m_providers;
  inline static int m_selectedPackageIndex = -1;
  inline static std::wstring m_currentStatusText = L"Ready";
  inline static bool m_isExecutingBackgroundAction = false;

  inline static std::map<PackageIdentifier, std::vector<PackageVersion>> m_versionsCache;
  inline static std::set<PackageIdentifier> m_pendingVersionQueries;

  // Demoscene Animation State
  inline static std::vector<RetroStarParticle> m_starfield;
  inline static std::wstring m_tickerText =
      L"  ***  SOFTWARE CENTER  ***  WITHOUT ADMIN OR SUDO  ***  EVOLVED WITH 1000:1 "
      L"READABILITY-FIRST METHODOLOGY  ***  CYBERPUNK RETRO DEMOSCENE TRON EDITION  ***  STAY "
      L"NATIVE, STAY COMPILED!  ***  ";
  inline static float m_tickerScrollOffset = 0.0f;
  inline static float m_sineWavePhase = 0.0f;
  inline static HFONT m_demosceneFont = nullptr;
  inline static HFONT m_scrollerFont = nullptr;
  inline static HBRUSH m_backgroundBrush = nullptr;
  inline static HBRUSH m_darkComboBoxBrush = nullptr;

  // High-DPI State
  inline static float m_dpiScaleFactor = 1.0f;
};

} // namespace CatUpdate

#endif // _WIN32
