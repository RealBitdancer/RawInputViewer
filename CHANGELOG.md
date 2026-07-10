# Changelog

All notable changes to this project are documented here.

## [1.1.0] - Unreleased

### Added

- **Input Device** list view column showing which keyboard sent each event, resolved via HID
  product string when available and a shortened device interface path otherwise
- Hot plug tracking through `RIDEV_DEVNOTIFY` and `WM_INPUT_DEVICE_CHANGE`
- `UniqueHandle` and `UniqueFileHandle` RAII wrappers for Win32 handles (trait based, in the
  spirit of WRL `HandleT`)
- CMake presets for MSVC x64 and x86 (`CMakePresets.json`)
- Single GitHub Actions Windows workflow (native MSVC, x64 and x86)
- GitHub Release workflow (tag `v*` on `main`) and a manual release dry run workflow
- README download instructions, control reference, and release notes
- This changelog, CONTRIBUTING.md, and SECURITY.md

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
