// ============================================================================
// include/vk_platform.hpp
// Ensure platform surface types are enabled for Vulkan-Hpp.
// Must be included BEFORE <vulkan/vulkan.hpp>.
// ============================================================================

#pragma once

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #   define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #   define NOMINMAX
    #endif
    #include <windows.h>
    #ifndef VK_USE_PLATFORM_WIN32_KHR
    #   define VK_USE_PLATFORM_WIN32_KHR
    #endif
#elif defined(__ANDROID__)
    #include <android/native_window.h>
    #ifndef VK_USE_PLATFORM_ANDROID_KHR
    #   define VK_USE_PLATFORM_ANDROID_KHR
    #endif
#elif defined(__linux__)
    #include <xcb/xcb.h>
    #ifndef VK_USE_PLATFORM_XCB_KHR
    #   define VK_USE_PLATFORM_XCB_KHR
    #endif
#endif
