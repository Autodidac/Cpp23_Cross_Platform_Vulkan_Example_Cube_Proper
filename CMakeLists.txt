cmake_minimum_required(VERSION 3.21)

project(vk_cross_platform_default LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VKMINI_ENABLE_VALIDATION "Enable validation layers if present" ON)
option(VKMINI_HEADLESS "Build/run without a window/swapchain" OFF)

add_executable(vulkan_app
  src/main.cpp
  src/vk_app.cpp
  src/vk_app_setup.cpp
  src/vk_app_run.cpp
  src/vk_device_select.cpp
  src/vk_helpers.cpp
  src/vk_validation.cpp
  src/math.cpp
  src/platform.cpp
  src/platform_win32.cpp
  src/platform_xcb.cpp
  src/platform_android.cpp
)

target_include_directories(vulkan_app PRIVATE include)

target_compile_definitions(vulkan_app PRIVATE
  VKMINI_ENABLE_VALIDATION=$<BOOL:${VKMINI_ENABLE_VALIDATION}>
  VKMINI_HEADLESS=$<BOOL:${VKMINI_HEADLESS}>
)

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(vulkan_app PRIVATE Vulkan::Vulkan)

# shaderc (GLSL->SPIR-V at runtime for the sample)
find_package(unofficial-shaderc CONFIG REQUIRED)
target_link_libraries(vulkan_app PRIVATE unofficial::shaderc::shaderc)

# Platform conditionals
if (WIN32)
  target_compile_definitions(vulkan_app PRIVATE VKMINI_PLATFORM_WIN32=1)
  target_sources(vulkan_app PRIVATE src/platform_win32.cpp)
elseif(ANDROID)
  target_compile_definitions(vulkan_app PRIVATE VKMINI_PLATFORM_ANDROID=1)
  target_sources(vulkan_app PRIVATE src/platform_android.cpp)
elseif(UNIX)
  target_compile_definitions(vulkan_app PRIVATE VKMINI_PLATFORM_XCB=1)
  target_sources(vulkan_app PRIVATE src/platform_xcb.cpp)
  find_package(XCB CONFIG QUIET)
  if (TARGET XCB::XCB)
    target_link_libraries(vulkan_app PRIVATE XCB::XCB)
  else()
    # Fallback: many distros provide -lxcb via system toolchain.
    target_link_libraries(vulkan_app PRIVATE xcb)
  endif()
endif()

# Avoid compiling unused platform units twice (already in list); keep them in sources but compile guards prevent issues.
