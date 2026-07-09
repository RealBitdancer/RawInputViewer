# RawInputViewer 

![](img/RawInputViewer.png)

A utility to test, visualize, and map WM_INPUT messages.

[![Build x86](https://github.com/RealBitdancer/RawInputViewer/actions/workflows/build_win_msvc_x86.yaml/badge.svg)](https://github.com/RealBitdancer/RawInputViewer/actions/workflows/build_win_msvc_x86.yaml)
[![Build x64](https://github.com/RealBitdancer/RawInputViewer/actions/workflows/build_win_msvc_x64.yaml/badge.svg)](https://github.com/RealBitdancer/RawInputViewer/actions/workflows/build_win_msvc_x64.yaml)

# How to Build

This project is written using **Visual Studio** with **C++23** enabled and utilizes new C++ features like concepts and ranges.

## What You Need
- **Visual Studio 2022 or newer**: Ensure the `Desktop development with C++` workload is installed.

## Steps
1. **Clone the code from GitHub**
   ```cmd
   git clone https://github.com/RealBitdancer/RawInputViewer.git && cd RawInputViewer
   ```
2. **Configure with CMake**

   Pick your flavor. The Visual Studio generator is resolved from the host default, so any installed version works.

* **64 Bit:**
   ```cmd
   cmake --preset default
   ```
* **32 Bit:**
   ```cmd
   cmake --preset msvc-x86
   ```
3. **Build**

   Either build from the command line:
   ```cmd
   cmake --build --preset debug
   ```
   Available build presets are `debug`, `release`, `x86-debug`, and `x86-release`.

   Or open the generated solution in Visual Studio, pick `Debug` or `Release`, then hit `F5` or `Ctrl+F5`:
   ```cmd
   start build\msvc-x64\RawInputViewer.sln
   ```
   Note: Newer CMake/Visual Studio versions may generate `RawInputViewer.slnx` instead of `RawInputViewer.sln`.

# Background
During my work on a personal platform abstraction library (SAL), I ran repeatedly into issues with WM_INPUT. To quickly test input on different systems, I put together a quick and dirty C++ Windows desktop app that was really only meant for myself. While reading up on the topic of WM_INPUT, I realized that this tool might be useful for other folks who struggle with the quirks of WM_INPUT, so I sat down and polished it a little to avoid completely embarrassing myself. So, here we are, enjoy `RawInputViewer`.

# Attribution

This project's icons and bitmaps have been sourced from and assembled with [Axialis IconWorshop](https://www.axialis.com/iconworkshop)
