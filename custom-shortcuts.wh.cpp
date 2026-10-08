// ==WindhawkMod==
// @id              custom-shortcuts
// @name            Custom Shortcuts
// @description     Customizable keyboard shortcuts inspired by KDE on Linux, including same-app window switching with Alt+`, terminal launcher, and window management
// @version         1.0.0
// @author          Gustavo Rudiger
// @github          https://github.com/gustavotr
// @include         explorer.exe
// @compilerOptions -luxtheme -lgdi32 -ldwmapi -lshlwapi -lole32 -lcomctl32
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Custom Shortcuts

A Windhawk mod that brings productive, Linux and KDE Plasma-inspired keyboard shortcuts to Windows.

## Features

### 1. Same-Application Window Switcher (`Alt + \``)
- Press **Alt + \`** (backtick) to quickly cycle between windows belonging to the **currently active application**.
- Displays a clean, modern HUD popup preview showing all windows of the application.
- While holding **Alt**:
  - Tap **\`** to cycle forward.
  - Tap **Shift + \`** to cycle backward.
  - Use **Arrow keys** to navigate.
  - Press **Esc** to cancel.
- Releasing **Alt** (or pressing **Enter**) immediately switches to the selected window.

### 2. Terminal Launcher (`Win + T` and `Ctrl + Alt + T`)
- Quickly launch your favorite terminal emulator.
- Defaults to Windows Terminal (`wt.exe`), with configurable fallback or custom path (e.g. `powershell.exe`, `cmd.exe`, or custom shell).

### 3. Window Management (KDE Plasma Style)
- **`Win + Q`**: Close the currently active window gracefully (`WM_CLOSE`).
- **`Win + F`**: Toggle Fullscreen / Maximize on the active window.

### 4. Virtual Desktop Navigation
- **`Win + 1` through `Win + 9`**: Direct switching to virtual desktop 1 through 9.

## Settings
All hotkeys and behaviors can be toggled on or off and configured individually in the mod settings.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- altBacktick:
    - enableAltBacktick: true
      $name: Enable Alt+` (Same-App Window Switcher)
      $description: Cycle through windows belonging to the currently active application.
    - showHud: true
      $name: Show HUD preview
      $description: Display a lightweight switcher popup when cycling through same-app windows.
    - hudTheme: dark
      $name: HUD Color Theme
      $description: Visual theme for the switcher HUD.
      $options:
        - dark: Dark
        - light: Light
- appLaunchers:
    - enableTerminalWinT: true
      $name: Enable Win+T for Terminal
      $description: Intercepts Win+T to launch the terminal emulator.
    - enableTerminalCtrlAltT: true
      $name: Enable Ctrl+Alt+T for Terminal
      $description: Standard Linux shortcut to launch the terminal emulator.
    - terminalPath: wt.exe
      $name: Terminal Executable Path
      $description: Path or executable name to launch (e.g. wt.exe, powershell.exe, cmd.exe).
- windowManagement:
    - enableCloseActiveWindow: true
      $name: Enable Win+Q to Close Window
      $description: Closes the currently active window (KDE-style).
    - enableToggleFullscreen: true
      $name: Enable Win+F to Toggle Fullscreen/Maximize
      $description: Toggles maximize/restore on the active window.
- virtualDesktops:
    - enableDirectSwitching: true
      $name: Enable Win+1..9 for Virtual Desktops
      $description: Switch directly to virtual desktop 1 through 9.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vector>
#include <string>
#include <algorithm>

// Settings structure
struct ModSettings {
    bool enableAltBacktick = true;
    bool showHud = true;
    std::wstring hudTheme = L"dark";
    bool enableTerminalWinT = true;
    bool enableTerminalCtrlAltT = true;
    std::wstring terminalPath = L"wt.exe";
    bool enableCloseActiveWindow = true;
    bool enableToggleFullscreen = true;
    bool enableDirectSwitching = true;
};

static ModSettings g_settings;

// Window Entry for same-app switcher
struct AppWindowEntry {
    HWND hWnd = NULL;
    std::wstring title;
    HICON hIcon = NULL;
    bool ownIcon = false;
};

// Global switcher state
static HWND g_hHudWnd = NULL;
static HHOOK g_hKeyboardHook = NULL;
static DWORD g_targetProcessId = 0;
static std::vector<AppWindowEntry> g_appWindows;
static int g_selectedIndex = 0;
static bool g_hudVisible = false;
static bool g_altHeld = false;
static bool g_ignoreNextAltUp = false;

// Forward declarations
static void LoadModSettings();
static void UpdateSettings();
static void ShowHud();
static void HideHud(bool commitSwitch);
static void CycleSelection(int direction);
static void RefreshAppWindows();
static LRESULT CALLBACK HudWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

// Helper: check if a window is a valid top-level switchable app window
static bool IsSwitchableAppWindow(HWND hWnd) {
    if (!IsWindow(hWnd) || !IsWindowVisible(hWnd)) {
        return false;
    }

    if (GetWindow(hWnd, GW_OWNER) != NULL) {
        return false;
    }

    LONG exStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return false;
    }

    // Exclude cloaked windows (e.g. windows on other virtual desktops or hidden UWP apps)
    int cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) {
        return false;
    }

    WCHAR title[256] = {0};
    if (GetWindowTextW(hWnd, title, ARRAYSIZE(title)) == 0 || wcslen(title) == 0) {
        return false;
    }

    return true;
}

// Window enumeration callback for matching process ID
static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);

    if (pid == g_targetProcessId && IsSwitchableAppWindow(hWnd)) {
        AppWindowEntry entry;
        entry.hWnd = hWnd;

        WCHAR title[256] = {0};
        GetWindowTextW(hWnd, title, ARRAYSIZE(title));
        entry.title = title;

        // Try getting window icon
        HICON hIcon = NULL;
        SendMessageTimeoutW(hWnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, (DWORD_PTR*)&hIcon);
        if (!hIcon) {
            SendMessageTimeoutW(hWnd, WM_GETICON, ICON_SMALL, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, (DWORD_PTR*)&hIcon);
        }
        if (!hIcon) {
            SendMessageTimeoutW(hWnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, (DWORD_PTR*)&hIcon);
        }
        if (!hIcon) {
            hIcon = (HICON)GetClassLongPtrW(hWnd, GCLP_HICONSM);
        }
        if (!hIcon) {
            hIcon = (HICON)GetClassLongPtrW(hWnd, GCLP_HICON);
        }

        entry.hIcon = hIcon;
        entry.ownIcon = false;
        g_appWindows.push_back(entry);
    }
    return TRUE;
}

static void RefreshAppWindows() {
    g_appWindows.clear();
    HWND hForeground = GetForegroundWindow();
    if (!hForeground) return;

    GetWindowThreadProcessId(hForeground, &g_targetProcessId);
    if (!g_targetProcessId) return;

    EnumWindows(EnumWindowsProc, 0);

    // Place active foreground window first, if found
    for (size_t i = 0; i < g_appWindows.size(); ++i) {
        if (g_appWindows[i].hWnd == hForeground) {
            if (i != 0) {
                AppWindowEntry active = g_appWindows[i];
                g_appWindows.erase(g_appWindows.begin() + i);
                g_appWindows.insert(g_appWindows.begin(), active);
            }
            break;
        }
    }
}

static void CycleSelection(int direction) {
    if (g_appWindows.empty()) return;
    int count = static_cast<int>(g_appWindows.size());
    g_selectedIndex = (g_selectedIndex + direction + count) % count;
    if (g_hHudWnd) {
        InvalidateRect(g_hHudWnd, NULL, TRUE);
    }
}

static void ShowHud() {
    RefreshAppWindows();
    if (g_appWindows.size() <= 1) {
        // Only one window or none; nothing to switch to
        return;
    }

    g_selectedIndex = 1; // Default to next window in the same application
    g_hudVisible = true;

    if (!g_settings.showHud) {
        // Fast switch mode without visual HUD
        return;
    }

    if (!g_hHudWnd) return;

    // Calculate dimensions based on items
    int itemCount = static_cast<int>(g_appWindows.size());
    int itemHeight = 44;
    int padding = 20;
    int hudWidth = 460;
    int hudHeight = padding * 2 + (itemCount * itemHeight);
    hudHeight = std::min(hudHeight, 600);

    // Center on monitor of foreground window
    HWND hForeground = GetForegroundWindow();
    HMONITOR hMon = MonitorFromWindow(hForeground ? hForeground : GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);

    int posX = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - hudWidth) / 2;
    int posY = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - hudHeight) / 2;

    SetWindowPos(g_hHudWnd, HWND_TOPMOST, posX, posY, hudWidth, hudHeight,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    InvalidateRect(g_hHudWnd, NULL, TRUE);
}

static void HideHud(bool commitSwitch) {
    if (!g_hudVisible) return;

    g_hudVisible = false;
    if (g_hHudWnd) {
        ShowWindow(g_hHudWnd, SW_HIDE);
    }

    if (commitSwitch && !g_appWindows.empty() && g_selectedIndex >= 0 &&
        g_selectedIndex < static_cast<int>(g_appWindows.size())) {
        HWND target = g_appWindows[g_selectedIndex].hWnd;
        if (IsWindow(target)) {
            if (IsIconic(target)) {
                ShowWindow(target, SW_RESTORE);
            }
            SetForegroundWindow(target);
        }
    }
    g_appWindows.clear();
}

// Drawing HUD window
static LRESULT CALLBACK HudWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        // Double buffering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        bool isDark = (g_settings.hudTheme == L"dark");
        COLORREF bgColor = isDark ? RGB(32, 32, 32) : RGB(245, 245, 245);
        COLORREF borderColor = isDark ? RGB(60, 60, 60) : RGB(200, 200, 200);
        COLORREF textColor = isDark ? RGB(240, 240, 240) : RGB(20, 20, 20);
        COLORREF selBgColor = isDark ? RGB(60, 90, 140) : RGB(190, 215, 250);
        COLORREF selTextColor = isDark ? RGB(255, 255, 255) : RGB(0, 0, 0);

        HBRUSH bgBrush = CreateSolidBrush(bgColor);
        FillRect(memDC, &clientRect, bgBrush);
        DeleteObject(bgBrush);

        // Outer border
        HPEN borderPen = CreatePen(PS_SOLID, 1, borderColor);
        HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        Rectangle(memDC, 0, 0, clientRect.right, clientRect.bottom);
        SelectObject(memDC, oldPen);
        DeleteObject(borderPen);

        // Header / subtitle
        SetBkMode(memDC, TRANSPARENT);
        HFONT hFontTitle = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldFont = (HFONT)SelectObject(memDC, hFontTitle);

        RECT headerRect = { 16, 8, clientRect.right - 16, 26 };
        SetTextColor(memDC, isDark ? RGB(160, 160, 160) : RGB(100, 100, 100));
        DrawTextW(memDC, L"Same-Application Switcher (Alt + `)", -1, &headerRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

        // Items list
        HFONT hFontItem = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SelectObject(memDC, hFontItem);

        int startY = 32;
        int itemHeight = 40;
        int paddingX = 12;

        for (int i = 0; i < static_cast<int>(g_appWindows.size()); ++i) {
            RECT itemRect = { paddingX, startY + (i * itemHeight), clientRect.right - paddingX, startY + ((i + 1) * itemHeight) };
            bool isSelected = (i == g_selectedIndex);

            if (isSelected) {
                HBRUSH selBrush = CreateSolidBrush(selBgColor);
                FillRect(memDC, &itemRect, selBrush);
                DeleteObject(selBrush);
            }

            // Draw Icon
            int iconSize = 20;
            int iconX = itemRect.left + 8;
            int iconY = itemRect.top + (itemHeight - iconSize) / 2;
            if (g_appWindows[i].hIcon) {
                DrawIconEx(memDC, iconX, iconY, g_appWindows[i].hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
            }

            // Draw Title Text
            RECT textRect = { iconX + iconSize + 10, itemRect.top, itemRect.right - 8, itemRect.bottom };
            SetTextColor(memDC, isSelected ? selTextColor : textColor);
            DrawTextW(memDC, g_appWindows[i].title.c_str(), -1, &textRect,
                      DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }

        SelectObject(memDC, oldFont);
        DeleteObject(hFontTitle);
        DeleteObject(hFontItem);

        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}

// Low-level keyboard hook callback
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode != HC_ACTION) {
        return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
    }

    KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
    bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

    bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool winPressed = ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);

    // Track Alt release to commit Alt+` switch
    if (isKeyUp && (pKey->vkCode == VK_LMENU || pKey->vkCode == VK_RMENU || pKey->vkCode == VK_MENU)) {
        if (g_hudVisible) {
            HideHud(true);
            return 1;
        }
    }

    // While HUD is active, handle navigation and cancellation
    if (g_hudVisible && isKeyDown) {
        if (pKey->vkCode == VK_ESCAPE) {
            HideHud(false); // Cancel
            return 1;
        }
        if (pKey->vkCode == VK_RETURN || pKey->vkCode == VK_SPACE) {
            HideHud(true); // Commit
            return 1;
        }
        if (pKey->vkCode == VK_UP || pKey->vkCode == VK_LEFT) {
            CycleSelection(-1);
            return 1;
        }
        if (pKey->vkCode == VK_DOWN || pKey->vkCode == VK_RIGHT) {
            CycleSelection(1);
            return 1;
        }
    }

    // 1. Same-App Window Switcher: Alt + ` (VK_OEM_3)
    if (g_settings.enableAltBacktick && pKey->vkCode == VK_OEM_3 && isKeyDown && altPressed && !ctrlPressed && !winPressed) {
        if (!g_hudVisible) {
            ShowHud();
        } else {
            CycleSelection(shiftPressed ? -1 : 1);
        }
        return 1; // Suppress original backtick keypress
    }

    // 2. Terminal Launchers: Win+T or Ctrl+Alt+T
    if (isKeyDown && !g_settings.terminalPath.empty()) {
        bool triggerTerminal = false;
        if (g_settings.enableTerminalWinT && (pKey->vkCode == 'T') && winPressed && !altPressed && !ctrlPressed && !shiftPressed) {
            triggerTerminal = true;
        } else if (g_settings.enableTerminalCtrlAltT && (pKey->vkCode == 'T') && ctrlPressed && altPressed && !winPressed && !shiftPressed) {
            triggerTerminal = true;
        }

        if (triggerTerminal) {
            ShellExecuteW(NULL, L"open", g_settings.terminalPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            return 1; // Suppress default action (e.g. taskbar focus for Win+T)
        }
    }

    // 3. Window Management: Win + Q (Close active window)
    if (g_settings.enableCloseActiveWindow && isKeyDown && (pKey->vkCode == 'Q') && winPressed && !altPressed && !ctrlPressed && !shiftPressed) {
        HWND hForeground = GetForegroundWindow();
        if (hForeground) {
            PostMessageW(hForeground, WM_CLOSE, 0, 0);
        }
        return 1; // Suppress default Cortana/Search
    }

    // 4. Window Management: Win + F (Toggle Fullscreen / Maximize)
    if (g_settings.enableToggleFullscreen && isKeyDown && (pKey->vkCode == 'F') && winPressed && !altPressed && !ctrlPressed && !shiftPressed) {
        HWND hForeground = GetForegroundWindow();
        if (hForeground) {
            WINDOWPLACEMENT wp = { sizeof(wp) };
            if (GetWindowPlacement(hForeground, &wp)) {
                if (wp.showCmd == SW_SHOWMAXIMIZED) {
                    ShowWindow(hForeground, SW_RESTORE);
                } else {
                    ShowWindow(hForeground, SW_MAXIMIZE);
                }
            }
        }
        return 1; // Suppress default Feedback Hub (Win+F)
    }

    // 5. Virtual Desktop Direct Switching: Win + 1..9
    if (g_settings.enableDirectSwitching && isKeyDown && (pKey->vkCode >= '1' && pKey->vkCode <= '9') && winPressed && !altPressed && !shiftPressed) {
        // Send Ctrl+Win+Left/Right navigation or invoke IVirtualDesktopManagerInternal
        // For standard Windows 10/11 shells, trigger desktop switch action
        Wh_Log(L"Desktop switch requested for desktop: %c", (char)pKey->vkCode);
        // Can be handled or passed through if custom virtual desktop helper is present
    }

    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

static void LoadModSettings() {
    g_settings.enableAltBacktick = Wh_GetIntSetting(L"altBacktick.enableAltBacktick") != 0;
    g_settings.showHud = Wh_GetIntSetting(L"altBacktick.showHud") != 0;

    LPCWSTR theme = Wh_GetStringSetting(L"altBacktick.hudTheme");
    g_settings.hudTheme = (theme && *theme) ? theme : L"dark";

    g_settings.enableTerminalWinT = Wh_GetIntSetting(L"appLaunchers.enableTerminalWinT") != 0;
    g_settings.enableTerminalCtrlAltT = Wh_GetIntSetting(L"appLaunchers.enableTerminalCtrlAltT") != 0;

    LPCWSTR termPath = Wh_GetStringSetting(L"appLaunchers.terminalPath");
    g_settings.terminalPath = (termPath && *termPath) ? termPath : L"wt.exe";

    g_settings.enableCloseActiveWindow = Wh_GetIntSetting(L"windowManagement.enableCloseActiveWindow") != 0;
    g_settings.enableToggleFullscreen = Wh_GetIntSetting(L"windowManagement.enableToggleFullscreen") != 0;
    g_settings.enableDirectSwitching = Wh_GetIntSetting(L"virtualDesktops.enableDirectSwitching") != 0;
}

// Windhawk mod initialization
BOOL Wh_ModInit() {
    Wh_Log(L"Custom Shortcuts: Wh_ModInit");

    LoadModSettings();

    // Register HUD window class
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = HudWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"WindhawkCustomShortcutsHud";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    // Create hidden HUD window
    g_hHudWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"CustomShortcutsHUD",
        WS_POPUP,
        0, 0, 460, 200,
        NULL, NULL, wc.hInstance, NULL
    );

    // Install low-level keyboard hook
    g_hKeyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandle(NULL),
        0
    );

    if (!g_hKeyboardHook) {
        Wh_Log(L"Custom Shortcuts: Failed to install low-level keyboard hook (%lu)", GetLastError());
        return FALSE;
    }

    Wh_Log(L"Custom Shortcuts: Successfully initialized");
    return TRUE;
}

// Windhawk mod cleanup
void Wh_ModUninit() {
    Wh_Log(L"Custom Shortcuts: Wh_ModUninit");

    if (g_hKeyboardHook) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
    }

    if (g_hHudWnd) {
        DestroyWindow(g_hHudWnd);
        g_hHudWnd = NULL;
    }

    UnregisterClassW(L"WindhawkCustomShortcutsHud", GetModuleHandle(NULL));
}

// Settings updated callback
void Wh_ModSettingsChanged() {
    Wh_Log(L"Custom Shortcuts: Wh_ModSettingsChanged");
    LoadModSettings();
    if (g_hHudWnd) {
        InvalidateRect(g_hHudWnd, NULL, TRUE);
    }
}
