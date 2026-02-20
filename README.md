# vk_cross_platform_default

Minimal Vulkan (C++23) sample with:
- vcpkg manifest mode (Vulkan + validation layers + shaderc)
- cross-platform window + surface creation:
  - Win32 (Windows)
  - XCB (Linux)
  - Android stub (scaffold only)

## Windows (MSVC)
1) Set vcpkg root for this terminal:
   `set VCPKG_ROOT=C:\vcpkg`
2) Run:
   `.\build.cmd`

This script enables validation if vcpkg installed `VkLayer_khronos_validation.json`.

## Linux
1) `export VCPKG_ROOT=/path/to/vcpkg`
2) `chmod +x build.sh`
3) `./build.sh Release`

Notes:
- The Linux backend uses XCB. Ensure X11/XCB runtime deps exist on your distro.
- vcpkg will pull `libxcb` on Linux via `vcpkg.json`.

## Android
`src/platform_android.cpp` is a scaffold only. Wiring a real Android `ANativeWindow` + event loop requires an NDK build and is intentionally left minimal here.
