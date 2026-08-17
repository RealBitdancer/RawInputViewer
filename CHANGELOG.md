# Changelog

All notable changes to this project are documented here.

## [1.1.1] - 2026-08-17

Highlights: first launch keeps the documented toolbar defaults, the event list no longer
grows without bound, and several shutdown and settings bugs are closed.

Settings stay under the existing `HKEY_CURRENT_USER` key for product version 1.1. A 1.1.0
install does not lose window placement or column layout.

### Fixed

- First launch (and any launch with no saved toolbar value) no longer unchecks **No Hotkeys**
  and **No Legacy**. Those toggles are restored only when a `ToolbarButtonStates` value
  exists.
- List-view image list is no longer destroyed twice on close. The control now uses
  `LVS_SHAREIMAGELISTS`.
- `GetRawInputData` failures no longer throw from the window procedure. Create-time
  failures return `-1` from `WM_CREATE`. A missing header popup menu no longer terminates
  the process.
- `PackedRawKeyboard` now zeroes the full `LPARAM` before packing fields, so the unused
  high half is defined on 64-bit builds.
- `WM_GETFONT` returning NULL is treated as the system font, not a fatal error.
- Registry settings are accepted only when the type is `REG_BINARY` and the size matches.
- `GetSubMenu` failure reports `ERROR_RESOURCE_NAME_NOT_FOUND` instead of a leftover
  last-error code.
- `constructRegistryKeyPath` is no longer marked `noexcept`. Allocation failure no longer
  calls `std::terminate`.
- 32-bit MSVC `/W4 /WX` no longer fails on `TempBuffer` alignment padding (`C4324`). The
  aligned scratch storage is now the first member.

### Changed

- The event list keeps the most recent 10,000 rows and drops the oldest.
- Unchecked **No Legacy** applies `RIDEV_NOLEGACY` to the keyboard only. Mouse `WM_*`
  messages stay enabled so the toolbar and list remain clickable. The README now says so.
- The application manifest declares Per-Monitor V2 DPI awareness. The main window handles
  `WM_DPICHANGED`.
- CMake minimum version is 3.23, matching `CMakePresets.json`. The executable sources are
  `PRIVATE`. Compile uses `/W4 /permissive-` and `/WX` in Debug and Release.

## [1.1.0] - 2026-07-09

Highlights: each keyboard event now shows which device sent it, and Windows binaries are
published on GitHub Releases.

### Added

- **Input Device column** in the list view. Each keyboard event now shows which physical device
  sent it. The name comes from the HID product string when available, otherwise from a shortened
  device interface path. Injected input, unknown devices, and the overflow case use fixed labels.
- Hot plug tracking when keyboards are connected or removed (`RIDEV_DEVNOTIFY`,
  `WM_INPUT_DEVICE_CHANGE`)
- Pre built Windows binaries on GitHub Releases (`RawInputViewer-x64.exe`, `RawInputViewer-x86.exe`)
- README download instructions and control reference
- This changelog, CONTRIBUTING.md, and SECURITY.md
- `UniqueHandle` and `UniqueFileHandle` RAII wrappers for Win32 handles (trait based, in the
  spirit of WRL `HandleT`)
- CMake presets for MSVC x64 and x86 (`CMakePresets.json`)
- Single GitHub Actions Windows workflow (native MSVC, x64 and x86)
- GitHub Release workflow (tag `v*` on `main`) and a manual release dry run workflow

### Fixed

- Extended scan codes from `MapVirtualKey` (`MAPVK_VK_TO_VSC_EX`) now set the E0 lookup flag
  correctly when the high byte is `0xE0`
- Left and right Ctrl and Alt are distinguished for both E0 and non E0 keys
- Adjusted make codes are drawn in bold (previously only virtual key adjustments were bold)
- Toolbar **No Hotkeys** and **No Legacy** toggles revert their checkbox state when
  `RegisterRawInputDevices` fails
- Column header format dropdown menus appear at the correct screen position
- `ImageList_Add` failure is handled instead of leaving a broken image list
- `RemoveWindowSubclass` is called on `WM_NCDESTROY`
- `LVN_GETDISPINFO` ignores items with a null text buffer or non positive length
- SAL library mapping label corrected from `Sml` to `Sal` in resources and code

### Changed

- C style casts replaced with `static_cast` and `reinterpret_cast` where appropriate
- Copyright line updated to 2025-2026 Bitdancer (github.com/RealBitdancer)
- GitHub Actions upgraded to Node 24 compatible action versions (`checkout@v5`, `upload-artifact@v5`,
  `download-artifact@v5`, `action-gh-release@v3`)

## [1.0.0] - 2025-04-07

### Added

- Windows desktop utility to test, visualize, and map `WM_INPUT` keyboard messages
- Live list view of virtual keys, scan codes, flags, and SAL, Raylib, and GLFW key name mappings
- **Adjust** mode to remap make codes and virtual keys for physical key positions (Pause/Break,
  Print Screen, left and right Ctrl and Alt, numpad Enter, and related edge cases)
- Toolbar toggles for `RIDEV_NOHOTKEYS` and `RIDEV_NOLEGACY`
- Per column decimal, hexadecimal, and binary display formats via header dropdowns
- Window placement and column layout persisted under `HKEY_CURRENT_USER`
- GitHub Actions CI build for Windows (MSVC)
- Embedded scan code and virtual key mapping tables as resources
