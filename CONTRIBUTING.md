# Contributing

Contributions are welcome. The project is small and intends to stay that way. The most useful
changes make WM_INPUT behavior easier to understand or more correct, not merely larger.

## What fits

**WM_INPUT accuracy fixes** are the most valuable submissions, and they should come with
evidence. A keyboard sequence that RawInputViewer displays wrongly, the raw fields you expected
compared with the ones shown, or a citation of the
[WM_INPUT documentation](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input) or
[RAWINPUT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinput) will
do. Another tool showing different values is a starting point, not proof. Microsoft's docs and a
reproducible key sequence settle the matter.

**Device identification improvements** (clearer names, hot plug edge cases, multiple keyboards)
are welcome when they stay lightweight and local to this tool.

**Bug fixes and robustness** matter too. The app registers for raw input, opens HID device paths
to read product strings, and reads and writes registry settings for window layout. Crashes, stuck
UI state, or resource leaks with clear repro steps are worth sending on their own.

**Build fixes, CI fixes, and documentation corrections** are welcome. CI is one Windows
workflow on `windows-latest` with native MSVC, building x64 and x86 Debug and Release. If it
stays green on `main` and `develop`, you have not broken anything we actually test.

**New features face a higher bar.** The tool visualizes and maps keyboard raw input. Features
that turn it into a general purpose input debugger, key remapper, or macro recorder will be
turned down.

## Building and testing

The README covers building. You need **Visual Studio 2022 or newer** with the Desktop
development with C++ workload, **CMake 3.23+**, and **C++23**.

```cmd
cmake --preset default
cmake --build --preset release
```

Build presets are `debug`, `release`, `x86-debug`, and `x86-release`. The GitHub Actions
workflow builds all four on every push to `main` and `develop`. Do the same before opening a
pull request.

There is no automated test suite yet. Manual verification on Windows with at least one physical
keyboard is expected, and a second keyboard if your change touches device display. Say what you
tested in the pull request.

## Style

`.clang-format` at the repository root defines the formatting. Match the surrounding code:
concepts, `enum class`, RAII wrappers for Win32 resources, and `std::` library types when they
read clearly.

Comment only what the code cannot say itself. A comment that repeats the next line will be
removed.

Zero third party dependencies is deliberate. The Windows SDK, CMake, and the C++ standard library
are the toolchain. Do not add vcpkg packages, NuGet libraries, or other external dependencies
without a strong reason.

## Licensing

The project is MIT. Contributions are accepted under the same terms.
