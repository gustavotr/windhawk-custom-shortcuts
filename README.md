# Custom Shortcuts (Windhawk Mod)

[![Windhawk Mod](https://img.shields.io/badge/Windhawk-Mod-blue.svg)](https://windhawk.net/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A [Windhawk](https://windhawk.net/) mod that brings productive, Linux and KDE Plasma-inspired keyboard shortcuts to Windows, featuring **fully customizable key combinations** for every action.

---

## Features & Configurable Shortcuts

You can freely set or customize the key combination for every single action in the Windhawk mod settings:

| Action | Default Shortcut | Behavior | Customization |
| :--- | :--- | :--- | :--- |
| **Same-App Window Switcher** | `Alt + \`` | Cycle through open windows of the currently active application with a clean HUD preview. | Custom hotkey (e.g. `Alt+Tab`, `Win+\``, `Alt+\``) |
| | `Alt + Shift + \`` | Cycle backwards through the same application's windows. | Automatic with Shift |
| | `Esc` / `Enter` | While holding `Alt`, press `Esc` to cancel or `Enter` to commit the switch. Releasing `Alt` also commits. | Built-in |
| **Custom App Launchers** | `Win + Enter` | Launch any configured application or command (defaults to `cmd.exe`). | Configurable shortcut list (e.g. `cmd.exe`, `wsl --cd ~`) |
| **Virtual Desktops** | `Win + 1 ... 9` | Direct access and navigation across virtual desktops. | Toggle setting |

*Tip: To disable any shortcut, simply clear its text in the settings.*

---

## Supported Key Formats

Hotkeys can be customized in the settings using standard combinations:
- **Modifiers**: `Alt`, `Ctrl`, `Shift`, `Win` (or `Super`/`Meta`)
- **Keys**:
  - Letters: `A` - `Z`
  - Numbers: `0` - `9`
  - Function keys: `F1` - `F24`
  - Special: `` ` `` (or `Backtick`/`Grave`/`Tilde`), `Tab`, `Space`, `Enter` (or `Return`), `Esc` (or `Escape`), `Backspace`, `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, `Up`, `Down`, `Left`, `Right`
  - Punctuation: `-`, `=`, `[`, `]`, `\`, `;`, `'`, `,`, `.`, `/`
  - Numeric virtual-key codes (e.g. `192` for backtick, `84` for T)

**Examples**:
- `Alt+` `
- `Win+T`
- `Ctrl+Alt+T`
- `Win+Q`
- `Alt+Q`
- `Win+W`
- `Win+Enter`
- `F11`

---

## How `Alt + \`` Same-Application Switcher Works

1. Pressing the configured hotkey (default `Alt + \``) identifies the executable and process ID of the currently focused window.
2. It gathers all visible top-level windows belonging to that application.
3. A native Alt+Tab-style preview displays the windows along with live DWM thumbnails, application icons, and titles.
4. Continuing to hold the modifier (e.g. `Alt`) and tapping `` ` `` (or `Shift + ` ``) cycles forward and backward through the windows.
5. Releasing `Alt` instantly brings the chosen window to the foreground.

---

## How to Install in Windhawk

1. Download and install [Windhawk](https://windhawk.net/).
2. Open Windhawk and click the **+ (Create Mod)** button on the top right (Developer Mode).
3. Copy the contents of [`custom-shortcuts.wh.cpp`](./custom-shortcuts.wh.cpp) and paste them into the code editor.
4. Click **Compile** and then **Save & Run**.
5. Customize your preferred hotkeys under the **Settings** tab. Changes take effect immediately without requiring a restart!

---

## Configuration & Settings

```yaml
- shortcuts:
    - hotkeySameApp: "Alt+`"
    - hotkeyCustomApp1: "Win+Enter"
    - hotkeyCustomApp2: ""
    - hotkeyCustomApp3: ""
- customAppOptions:
    - customAppPath1: "cmd.exe"
    - customAppPath2: ""
    - customAppPath3: ""
- hudOptions:
    - showHud: true
    - hudTheme: dark
- virtualDesktops:
    - enableDirectSwitching: true
    - desktopModifier: "win"
    - windowsVersion: "auto"
```

---

## License

This project is licensed under the [MIT License](LICENSE).
