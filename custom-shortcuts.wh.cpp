// ==WindhawkMod==
// @id              custom-shortcuts
// @name            Custom Shortcuts
// @description     Customizable keyboard shortcuts inspired by KDE on Linux, including same-app window switching with Alt+`, terminal launcher, and window management
// @version         1.1.1
// @author          Gustavo Rudiger
// @github          https://github.com/gustavotr
// @include         explorer.exe
// @compilerOptions -luxtheme -lgdi32 -ldwmapi -lshlwapi -lole32 -lcomctl32
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Custom Shortcuts

A Windhawk mod that brings productive, Linux and KDE Plasma-inspired keyboard shortcuts to Windows, with **fully customizable key combinations** for every action.

## Configurable Keybindings

You can customize the key combination for every single action in the mod settings according to your personal preference:

| Action | Default Combination | Description |
| :--- | :--- | :--- |
| **Same-App Window Switcher** | `Alt + \`` | Cycle through open windows of the currently active application with a clean HUD preview. |
| **Terminal Launcher (Primary)** | `Win + T` | Quickly launch your preferred terminal emulator (defaults to Windows Terminal `wt.exe`). |
| **Terminal Launcher (Secondary)**| `Ctrl + Alt + T` | Standard Linux shortcut to launch the terminal emulator. |
| **Close Active Window** | `Win + Q` | Closes the currently active window gracefully (`WM_CLOSE`), KDE Plasma-style. |
| **Toggle Fullscreen / Maximize**| `Win + F` | Toggles maximize / restore on the active window. |
| **Virtual Desktop Navigation** | `Win + 1 ... 9` | Direct switching to virtual desktop 1 through 9. |

*To disable any shortcut, simply clear its setting field.*

## Supported Key Formats

Key combinations are specified in the format `Modifier+Key` or `Modifier+Modifier+Key`.
- **Modifiers**: `Alt`, `Ctrl`, `Shift`, `Win`
- **Keys**: Letters (`A-Z`), Numbers (`0-9`), Function keys (`F1-F24`), Backtick (`` ` ``), `Tab`, `Space`, `Enter`, `Esc`, `Home`, `End`, `Delete`, Arrows, etc.
- **Examples**: `Alt+` `, `Win+T`, `Ctrl+Alt+T`, `Win+Q`, `Alt+Q`, `Win+W`, `Win+Enter`, `F11`.

## How the Same-App Switcher Works

1. Press your configured hotkey (default `Alt + \``) to focus and list only windows of the active application.
2. A lightweight HUD popup displays all matching windows with their application icons and titles.
3. Keep the modifier (e.g. `Alt`) held down and tap the key (or `Shift + key`) to cycle forward and backward.
4. Use arrow keys (`Up`/`Down`/`Left`/`Right`) to browse. Press `Esc` to cancel.
5. Release the modifier (or press `Enter`) to switch to the selected window.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- shortcuts:
    - hotkeySameApp: "Alt+`"
      $name: Same-App Window Switcher
      $description: >-
        Key combination to cycle windows of the active application (e.g. Alt+`, Alt+Tab, Win+`). Leave empty to disable.
    - hotkeyTerminalWinT: "Win+T"
      $name: Terminal Shortcut (Primary)
      $description: >-
        Key combination to launch terminal (e.g. Win+T, Win+Enter). Leave empty to disable.
    - hotkeyTerminalCtrlAltT: "Ctrl+Alt+T"
      $name: Terminal Shortcut (Secondary)
      $description: >-
        Secondary shortcut to launch terminal (e.g. Ctrl+Alt+T). Leave empty to disable.
    - hotkeyCloseWindow: "Win+Q"
      $name: Close Active Window
      $description: >-
        Key combination to close active window (e.g. Win+Q, Alt+Q, Win+W). Leave empty to disable.
    - hotkeyToggleFullscreen: "Win+F"
      $name: Toggle Fullscreen / Maximize
      $description: >-
        Key combination to toggle fullscreen / maximize (e.g. Win+F, F11, Alt+Enter). Leave empty to disable.
- terminalOptions:
    - terminalPath: "wt.exe"
      $name: Terminal Executable Path
      $description: Path or executable name to launch (e.g. wt.exe, powershell.exe, cmd.exe).
- hudOptions:
    - showHud: true
      $name: Show HUD preview
      $description: Show visual switcher preview popup when switching same-app windows.
    - hudTheme: dark
      $name: HUD Color Theme
      $description: Visual theme for the switcher HUD.
      $options:
        - dark: Dark
        - light: Light
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
#include <cwctype>

// Hotkey binding representation
struct HotkeyBinding {
    UINT modifiers = 0; // MOD_ALT, MOD_CONTROL, MOD_SHIFT, MOD_WIN
    UINT vk = 0;        // Virtual key code
    bool valid = false;
    std::wstring originalStr;

    bool Matches(UINT activeModifiers, UINT pressedVk) const {
        if (!valid) return false;
        UINT cleanActiveMods = activeModifiers & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN);
        UINT cleanTargetMods = modifiers & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN);
        return (cleanTargetMods == cleanActiveMods) && (vk == pressedVk);
    }
};

// Mod settings structure
struct ModSettings {
    HotkeyBinding bindingSameApp;
    HotkeyBinding bindingTerminalWinT;
    HotkeyBinding bindingTerminalCtrlAltT;
    HotkeyBinding bindingCloseWindow;
    HotkeyBinding bindingToggleFullscreen;

    std::wstring terminalPath = L"wt.exe";
    bool showHud = true;
    std::wstring hudTheme = L"dark";
    bool enableDirectSwitching = true;
};

static ModSettings g_settings;

// Window Entry for same-app switcher
struct AppWindowEntry {
    HWND hWnd = NULL;
    std::wstring title;
    HICON hIcon = NULL;
};

// Global state
static HANDLE g_hHookThread = NULL;
static DWORD g_dwHookThreadId = 0;
static HWND g_hHudWnd = NULL;
static HHOOK g_hKeyboardHook = NULL;
static DWORD g_targetProcessId = 0;
static std::vector<AppWindowEntry> g_appWindows;
static int g_selectedIndex = 0;
static bool g_hudVisible = false;
static UINT g_hudTriggerModifier = MOD_ALT;

// Forward declarations
static bool ParseHotkeyString(const std::wstring& str, HotkeyBinding& out);
static void LoadModSettings();
static void ShowHud();
static void HideHud(bool commitSwitch);
static void CycleSelection(int direction);
static void RefreshAppWindows();
static DWORD WINAPI HookThreadProc(LPVOID lpParam);
static LRESULT CALLBACK HudWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

// Parse hotkey strings like "Alt+`", "Win+T", "ctrl+alt+84", "Win+Q", "F11"
static bool ParseHotkeyString(const std::wstring& str, HotkeyBinding& out) {
    out = HotkeyBinding();
    out.originalStr = str;
    if (str.empty()) return false;

    std::vector<std::wstring> tokens;
    size_t start = 0;
    while (start < str.length()) {
        size_t plusPos = str.find(L'+', start);
        if (plusPos == std::wstring::npos) {
            tokens.push_back(str.substr(start));
            break;
        }
        tokens.push_back(str.substr(start, plusPos - start));
        start = plusPos + 1;
    }

    if (tokens.empty()) return false;

    UINT mods = 0;
    UINT vk = 0;

    for (size_t i = 0; i < tokens.size(); ++i) {
        std::wstring token = tokens[i];
        while (!token.empty() && (token.front() == L' ' || token.front() == L'\t')) token.erase(0, 1);
        while (!token.empty() && (token.back() == L' ' || token.back() == L'\t')) token.pop_back();
        if (token.empty()) continue;

        std::wstring upper = token;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::towupper);

        if (upper == L"CTRL" || upper == L"CONTROL") {
            mods |= MOD_CONTROL;
        } else if (upper == L"ALT" || upper == L"MENU") {
            mods |= MOD_ALT;
        } else if (upper == L"SHIFT") {
            mods |= MOD_SHIFT;
        } else if (upper == L"WIN" || upper == L"WINDOWS" || upper == L"SUPER" || upper == L"META") {
            mods |= MOD_WIN;
        } else {
            bool allDigits = true;
            for (wchar_t c : upper) {
                if (!iswdigit(c)) { allDigits = false; break; }
            }

            if (allDigits && !upper.empty()) {
                vk = static_cast<UINT>(_wtoi(upper.c_str()));
            } else if (upper == L"`" || upper == L"~" || upper == L"BACKTICK" || upper == L"GRAVE" || upper == L"TILDE") {
                vk = VK_OEM_3;
            } else if (upper == L"TAB") {
                vk = VK_TAB;
            } else if (upper == L"ENTER" || upper == L"RETURN") {
                vk = VK_RETURN;
            } else if (upper == L"ESC" || upper == L"ESCAPE") {
                vk = VK_ESCAPE;
            } else if (upper == L"SPACE" || upper == L"SPACEBAR") {
                vk = VK_SPACE;
            } else if (upper == L"BACKSPACE") {
                vk = VK_BACK;
            } else if (upper == L"DELETE" || upper == L"DEL") {
                vk = VK_DELETE;
            } else if (upper == L"INSERT" || upper == L"INS") {
                vk = VK_INSERT;
            } else if (upper == L"HOME") {
                vk = VK_HOME;
            } else if (upper == L"END") {
                vk = VK_END;
            } else if (upper == L"PAGEUP" || upper == L"PGUP") {
                vk = VK_PRIOR;
            } else if (upper == L"PAGEDOWN" || upper == L"PGDN") {
                vk = VK_NEXT;
            } else if (upper == L"UP") {
                vk = VK_UP;
            } else if (upper == L"DOWN") {
                vk = VK_DOWN;
            } else if (upper == L"LEFT") {
                vk = VK_LEFT;
            } else if (upper == L"RIGHT") {
                vk = VK_RIGHT;
            } else if (upper.length() >= 2 && upper[0] == L'F' && iswdigit(upper[1])) {
                int fNum = _wtoi(upper.c_str() + 1);
                if (fNum >= 1 && fNum <= 24) {
                    vk = VK_F1 + (fNum - 1);
                }
            } else if (upper.length() == 1) {
                wchar_t c = upper[0];
                if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
                    vk = static_cast<UINT>(c);
                } else if (c == L'-' || c == L'_') {
                    vk = VK_OEM_MINUS;
                } else if (c == L'=' || c == L'+') {
                    vk = VK_OEM_PLUS;
                } else if (c == L'[' || c == L'{') {
                    vk = VK_OEM_4;
                } else if (c == L']' || c == L'}') {
                    vk = VK_OEM_6;
                } else if (c == L'\\' || c == L'|') {
                    vk = VK_OEM_5;
                } else if (c == L';' || c == L':') {
                    vk = VK_OEM_1;
                } else if (c == L'\'' || c == L'\"') {
                    vk = VK_OEM_7;
                } else if (c == L',' || c == L'<') {
                    vk = VK_OEM_COMMA;
                } else if (c == L'.' || c == L'>') {
                    vk = VK_OEM_PERIOD;
                } else if (c == L'/' || c == L'?') {
                    vk = VK_OEM_2;
                }
            }
        }
    }

    if (vk != 0) {
        out.modifiers = mods;
        out.vk = vk;
        out.valid = true;
        return true;
    }
    return false;
}

// Check if a window is a valid top-level switchable window
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
    Wh_Log(L"[CustomShortcuts] Cycling selection: %d of %d", g_selectedIndex + 1, count);
    if (g_hHudWnd) {
        InvalidateRect(g_hHudWnd, NULL, TRUE);
    }
}

static void ShowHud() {
    RefreshAppWindows();
    Wh_Log(L"[CustomShortcuts] Found %d window(s) for target process PID %lu",
           (int)g_appWindows.size(), g_targetProcessId);

    if (g_appWindows.size() <= 1) {
        Wh_Log(L"[CustomShortcuts] Only 1 window found for this app, nothing to cycle.");
        return;
    }

    g_selectedIndex = 1; // Default to next window in list
    g_hudVisible = true;

    if (!g_settings.showHud) {
        return;
    }

    if (!g_hHudWnd) return;

    int itemCount = static_cast<int>(g_appWindows.size());
    int itemHeight = 44;
    int padding = 20;
    int hudWidth = 460;
    int hudHeight = padding * 2 + (itemCount * itemHeight);
    hudHeight = std::min(hudHeight, 600);

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
        Wh_Log(L"[CustomShortcuts] Switching to window: %p (\"%s\")",
               target, g_appWindows[g_selectedIndex].title.c_str());
        if (IsWindow(target)) {
            if (IsIconic(target)) {
                ShowWindow(target, SW_RESTORE);
            }
            SetForegroundWindow(target);
        }
    } else {
        Wh_Log(L"[CustomShortcuts] Switch cancelled.");
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

        HPEN borderPen = CreatePen(PS_SOLID, 1, borderColor);
        HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        Rectangle(memDC, 0, 0, clientRect.right, clientRect.bottom);
        SelectObject(memDC, oldPen);
        DeleteObject(borderPen);

        SetBkMode(memDC, TRANSPARENT);
        HFONT hFontTitle = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldFont = (HFONT)SelectObject(memDC, hFontTitle);

        RECT headerRect = { 16, 8, clientRect.right - 16, 26 };
        SetTextColor(memDC, isDark ? RGB(160, 160, 160) : RGB(100, 100, 100));
        DrawTextW(memDC, L"Same-Application Switcher", -1, &headerRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

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

            int iconSize = 20;
            int iconX = itemRect.left + 8;
            int iconY = itemRect.top + (itemHeight - iconSize) / 2;
            if (g_appWindows[i].hIcon) {
                DrawIconEx(memDC, iconX, iconY, g_appWindows[i].hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
            }

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

    // Compute active modifiers
    UINT activeModifiers = 0;
    if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) activeModifiers |= MOD_ALT;
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) activeModifiers |= MOD_CONTROL;
    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) activeModifiers |= MOD_SHIFT;
    if (((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0)) activeModifiers |= MOD_WIN;

    // Log key press if any modifier is active or if it matches special keys
    if (isKeyDown && activeModifiers != 0) {
        Wh_Log(L"[CustomShortcuts] Key pressed: vk=0x%02X (%u), active mods: [Win=%d, Alt=%d, Ctrl=%d, Shift=%d]",
               pKey->vkCode, pKey->vkCode,
               (activeModifiers & MOD_WIN) != 0,
               (activeModifiers & MOD_ALT) != 0,
               (activeModifiers & MOD_CONTROL) != 0,
               (activeModifiers & MOD_SHIFT) != 0);
    }

    // Track modifier release to commit HUD selection
    if (isKeyUp && g_hudVisible) {
        bool modifierReleased = false;
        if ((g_hudTriggerModifier & MOD_ALT) && (pKey->vkCode == VK_LMENU || pKey->vkCode == VK_RMENU || pKey->vkCode == VK_MENU)) {
            modifierReleased = true;
        } else if ((g_hudTriggerModifier & MOD_CONTROL) && (pKey->vkCode == VK_LCONTROL || pKey->vkCode == VK_RCONTROL || pKey->vkCode == VK_CONTROL)) {
            modifierReleased = true;
        } else if ((g_hudTriggerModifier & MOD_WIN) && (pKey->vkCode == VK_LWIN || pKey->vkCode == VK_RWIN)) {
            modifierReleased = true;
        }

        if (modifierReleased) {
            Wh_Log(L"[CustomShortcuts] Modifier released, committing switch.");
            HideHud(true);
            return 1;
        }
    }

    // While HUD is visible, handle navigation and cancellation
    if (g_hudVisible && isKeyDown) {
        if (pKey->vkCode == VK_ESCAPE) {
            Wh_Log(L"[CustomShortcuts] ESC pressed, cancelling switch.");
            HideHud(false);
            return 1;
        }
        if (pKey->vkCode == VK_RETURN || pKey->vkCode == VK_SPACE) {
            Wh_Log(L"[CustomShortcuts] Enter/Space pressed, committing switch.");
            HideHud(true);
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

    // 1. Same-App Window Switcher
    if (isKeyDown && g_settings.bindingSameApp.valid) {
        UINT modsWithoutShift = activeModifiers & ~MOD_SHIFT;
        UINT targetModsWithoutShift = g_settings.bindingSameApp.modifiers & ~MOD_SHIFT;

        if (pKey->vkCode == g_settings.bindingSameApp.vk && modsWithoutShift == targetModsWithoutShift) {
            Wh_Log(L"[CustomShortcuts] >>> TRIGGERED Same-App Window Switcher! (%s)",
                   g_settings.bindingSameApp.originalStr.c_str());
            g_hudTriggerModifier = g_settings.bindingSameApp.modifiers ? g_settings.bindingSameApp.modifiers : MOD_ALT;
            if (!g_hudVisible) {
                ShowHud();
            } else {
                bool isShift = (activeModifiers & MOD_SHIFT) != 0;
                CycleSelection(isShift ? -1 : 1);
            }
            return 1;
        }
    }

    // 2. Terminal Launchers (Primary and Secondary)
    if (isKeyDown && !g_settings.terminalPath.empty()) {
        if (g_settings.bindingTerminalWinT.Matches(activeModifiers, pKey->vkCode) ||
            g_settings.bindingTerminalCtrlAltT.Matches(activeModifiers, pKey->vkCode)) {
            Wh_Log(L"[CustomShortcuts] >>> TRIGGERED Terminal Launcher! Launching: %s",
                   g_settings.terminalPath.c_str());
            ShellExecuteW(NULL, L"open", g_settings.terminalPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            return 1;
        }
    }

    // 3. Close Active Window
    if (isKeyDown && g_settings.bindingCloseWindow.Matches(activeModifiers, pKey->vkCode)) {
        Wh_Log(L"[CustomShortcuts] >>> TRIGGERED Close Active Window! (%s)",
               g_settings.bindingCloseWindow.originalStr.c_str());
        HWND hForeground = GetForegroundWindow();
        if (hForeground) {
            PostMessageW(hForeground, WM_CLOSE, 0, 0);
        }
        return 1;
    }

    // 4. Toggle Fullscreen / Maximize
    if (isKeyDown && g_settings.bindingToggleFullscreen.Matches(activeModifiers, pKey->vkCode)) {
        Wh_Log(L"[CustomShortcuts] >>> TRIGGERED Toggle Fullscreen/Maximize! (%s)",
               g_settings.bindingToggleFullscreen.originalStr.c_str());
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
        return 1;
    }

    // 5. Virtual Desktop Direct Switching: Win + 1..9
    if (g_settings.enableDirectSwitching && isKeyDown && (pKey->vkCode >= '1' && pKey->vkCode <= '9') &&
        (activeModifiers == MOD_WIN)) {
        Wh_Log(L"[CustomShortcuts] >>> TRIGGERED Virtual Desktop switch: Desktop %c", (char)pKey->vkCode);
    }

    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

// Dedicated background thread running message loop for WH_KEYBOARD_LL
static DWORD WINAPI HookThreadProc(LPVOID lpParam) {
    Wh_Log(L"[CustomShortcuts] Hook thread started (ID: %lu)", GetCurrentThreadId());

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = HudWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"WindhawkCustomShortcutsHud";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    g_hHudWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"CustomShortcutsHUD",
        WS_POPUP,
        0, 0, 460, 200,
        NULL, NULL, wc.hInstance, NULL
    );

    g_hKeyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandle(NULL),
        0
    );

    if (!g_hKeyboardHook) {
        g_hKeyboardHook = SetWindowsHookExW(
            WH_KEYBOARD_LL,
            LowLevelKeyboardProc,
            NULL,
            0
        );
    }

    if (!g_hKeyboardHook) {
        Wh_Log(L"[CustomShortcuts] ERROR: Failed to install WH_KEYBOARD_LL hook (Error %lu)", GetLastError());
        return 1;
    }

    Wh_Log(L"[CustomShortcuts] SUCCESS: WH_KEYBOARD_LL hook installed and listening to keyboard events!");

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Wh_Log(L"[CustomShortcuts] Hook thread message loop terminating...");

    if (g_hKeyboardHook) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
    }

    if (g_hHudWnd) {
        DestroyWindow(g_hHudWnd);
        g_hHudWnd = NULL;
    }

    UnregisterClassW(L"WindhawkCustomShortcutsHud", GetModuleHandle(NULL));
    return 0;
}

// Load and parse all mod settings
static void LoadModSettings() {
    LPCWSTR sameAppStr = Wh_GetStringSetting(L"shortcuts.hotkeySameApp");
    ParseHotkeyString((sameAppStr && *sameAppStr) ? sameAppStr : L"Alt+`", g_settings.bindingSameApp);

    LPCWSTR termWinStr = Wh_GetStringSetting(L"shortcuts.hotkeyTerminalWinT");
    ParseHotkeyString((termWinStr && *termWinStr) ? termWinStr : L"Win+T", g_settings.bindingTerminalWinT);

    LPCWSTR termCtrlAltStr = Wh_GetStringSetting(L"shortcuts.hotkeyTerminalCtrlAltT");
    ParseHotkeyString((termCtrlAltStr && *termCtrlAltStr) ? termCtrlAltStr : L"Ctrl+Alt+T", g_settings.bindingTerminalCtrlAltT);

    LPCWSTR closeStr = Wh_GetStringSetting(L"shortcuts.hotkeyCloseWindow");
    ParseHotkeyString((closeStr && *closeStr) ? closeStr : L"Win+Q", g_settings.bindingCloseWindow);

    LPCWSTR fsStr = Wh_GetStringSetting(L"shortcuts.hotkeyToggleFullscreen");
    ParseHotkeyString((fsStr && *fsStr) ? fsStr : L"Win+F", g_settings.bindingToggleFullscreen);

    LPCWSTR termPath = Wh_GetStringSetting(L"terminalOptions.terminalPath");
    g_settings.terminalPath = (termPath && *termPath) ? termPath : L"wt.exe";

    g_settings.showHud = Wh_GetIntSetting(L"hudOptions.showHud") != 0;

    LPCWSTR theme = Wh_GetStringSetting(L"hudOptions.hudTheme");
    g_settings.hudTheme = (theme && *theme) ? theme : L"dark";

    g_settings.enableDirectSwitching = Wh_GetIntSetting(L"virtualDesktops.enableDirectSwitching") != 0;

    Wh_Log(L"[CustomShortcuts] Settings loaded:");
    Wh_Log(L"[CustomShortcuts]   Same-App: '%s' (valid=%d, vk=0x%02X, mods=0x%X)",
           g_settings.bindingSameApp.originalStr.c_str(), g_settings.bindingSameApp.valid,
           g_settings.bindingSameApp.vk, g_settings.bindingSameApp.modifiers);
    Wh_Log(L"[CustomShortcuts]   Terminal 1: '%s' (valid=%d)",
           g_settings.bindingTerminalWinT.originalStr.c_str(), g_settings.bindingTerminalWinT.valid);
    Wh_Log(L"[CustomShortcuts]   Terminal 2: '%s' (valid=%d)",
           g_settings.bindingTerminalCtrlAltT.originalStr.c_str(), g_settings.bindingTerminalCtrlAltT.valid);
    Wh_Log(L"[CustomShortcuts]   Close Win: '%s' (valid=%d)",
           g_settings.bindingCloseWindow.originalStr.c_str(), g_settings.bindingCloseWindow.valid);
    Wh_Log(L"[CustomShortcuts]   Fullscreen: '%s' (valid=%d)",
           g_settings.bindingToggleFullscreen.originalStr.c_str(), g_settings.bindingToggleFullscreen.valid);
}

// Windhawk mod initialization
BOOL Wh_ModInit() {
    Wh_Log(L"[CustomShortcuts] Wh_ModInit called");

    LoadModSettings();

    // Spawn dedicated thread with message loop for the WH_KEYBOARD_LL hook
    g_hHookThread = CreateThread(NULL, 0, HookThreadProc, NULL, 0, &g_dwHookThreadId);
    if (!g_hHookThread) {
        Wh_Log(L"[CustomShortcuts] ERROR: CreateThread failed (Error %lu)", GetLastError());
        return FALSE;
    }

    Wh_Log(L"[CustomShortcuts] Hook thread created (Thread ID %lu)", g_dwHookThreadId);
    return TRUE;
}

// Windhawk mod cleanup
void Wh_ModUninit() {
    Wh_Log(L"[CustomShortcuts] Wh_ModUninit called");

    if (g_dwHookThreadId) {
        PostThreadMessageW(g_dwHookThreadId, WM_QUIT, 0, 0);
        if (g_hHookThread) {
            WaitForSingleObject(g_hHookThread, 2000);
            CloseHandle(g_hHookThread);
            g_hHookThread = NULL;
        }
        g_dwHookThreadId = 0;
    }

    Wh_Log(L"[CustomShortcuts] Cleanup complete.");
}

// Settings updated callback
void Wh_ModSettingsChanged() {
    Wh_Log(L"[CustomShortcuts] Wh_ModSettingsChanged called");
    LoadModSettings();
    if (g_hHudWnd) {
        InvalidateRect(g_hHudWnd, NULL, TRUE);
    }
}
