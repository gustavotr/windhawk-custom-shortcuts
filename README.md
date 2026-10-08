# Custom Shortcuts (Windhawk Mod)

[![Windhawk Mod](https://img.shields.io/badge/Windhawk-Mod-blue.svg)](https://windhawk.net/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A [Windhawk](https://windhawk.net/) mod that brings productive, Linux and KDE Plasma-inspired keyboard shortcuts to Windows, including same-application window switching with `Alt + \``, terminal launcher shortcuts, and window management.

---

## Features & Shortcuts

| Category | Shortcut | Behavior | Configurable Setting |
| :--- | :--- | :--- | :--- |
| **Window Switching** | `Alt + \`` | **Same-Application Switcher**: Cycle through open windows of the currently active application with a clean HUD preview. | `enableAltBacktick`, `showHud`, `hudTheme` |
| | `Alt + Shift + \`` | Cycle backwards through the same application's windows. | Enabled with `Alt + \`` |
| | `Esc` / `Enter` | While holding `Alt`, press `Esc` to cancel or `Enter` to commit the switch. Releasing `Alt` also commits. | Built-in |
| **App Launching** | `Win + T` | Launch Terminal (defaults to Windows Terminal `wt.exe`). Suppresses Windows taskbar focus. | `enableTerminalWinT`, `terminalPath` |
| | `Ctrl + Alt + T` | Linux standard shortcut to launch Terminal. | `enableTerminalCtrlAltT`, `terminalPath` |
| **Window Control** | `Win + Q` | Close the currently focused window gracefully (`WM_CLOSE`), similar to KDE Plasma. | `enableCloseActiveWindow` |
| | `Win + F` | Toggle Fullscreen / Maximize on the active window. | `enableToggleFullscreen` |
| **Virtual Desktops** | `Win + 1 ... 9` | Direct access and navigation across virtual desktops. | `enableDirectSwitching` |

---

## How `Alt + \`` Same-Application Switcher Works

1. Pressing `Alt + \`` identifies the executable and process ID of the currently focused window.
2. It gathers all visible top-level windows belonging to that application.
3. A sleek, unobtrusive HUD preview displays the windows along with their application icons and titles.
4. Continuing to hold `Alt` and tapping `\`` (or `Shift + \``) cycles forward and backward through the windows.
5. Releasing `Alt` instantly brings the chosen window to the foreground.

---

## How to Install in Windhawk

1. Download and install [Windhawk](https://windhawk.net/).
2. Open Windhawk and click the **+ (Create Mod)** button on the top right (Developer Mode).
3. Copy the contents of [`custom-shortcuts.wh.cpp`](./custom-shortcuts.wh.cpp) and paste them into the code editor.
4. Click **Compile** and then **Save & Run**.
5. Once compiled, you can customize any settings under the **Settings** tab.

---

## Configuration & Settings

All shortcuts can be independently enabled or disabled in the Windhawk mod settings:

```yaml
- altBacktick:
    - enableAltBacktick: true    # Enable/disable Alt+`
    - showHud: true              # Toggle HUD preview window
    - hudTheme: dark             # "dark" or "light"
- appLaunchers:
    - enableTerminalWinT: true   # Enable Win+T
    - enableTerminalCtrlAltT: true # Enable Ctrl+Alt+T
    - terminalPath: wt.exe       # Path to terminal (e.g. wt.exe, powershell.exe)
- windowManagement:
    - enableCloseActiveWindow: true # Enable Win+Q
    - enableToggleFullscreen: true # Enable Win+F
- virtualDesktops:
    - enableDirectSwitching: true # Enable Win+1..9
```

---

## Submitting to Windhawk Repository

This mod is designed to conform to the official [Windhawk Mods repository guidelines](https://github.com/ramensoftware/windhawk-mods):
- Includes all standard `@id`, `@name`, `@description`, `@version`, `@author`, and `@include` metadata.
- Targets `explorer.exe` for lightweight integration with the Windows desktop shell.
- Includes embedded `WindhawkModReadme` and `WindhawkModSettings`.

---

## License

This project is licensed under the [MIT License](LICENSE).
