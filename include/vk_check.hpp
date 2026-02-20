// ============================================================================
// include/vk_check.hpp
// Vulkan-Hpp error handling shim.
//
// If VULKAN_HPP_NO_EXCEPTIONS is defined, many Vulkan-Hpp calls return vk::Result
// (or vk::ResultValue<T>) and are marked [[nodiscard]].
// If exceptions are enabled, failures throw and many calls return void.
// ============================================================================

#pragma once

#include <vulkan/vulkan.hpp>

#include <stdexcept>
#include <string>

namespace vkmini
{
    inline void vk_check(vk::Result r, const char* what)
    {
        if (r != vk::Result::eSuccess)
            throw std::runtime_error(std::string(what) + " failed: " + vk::to_string(r));
    }

    template <class T>
    inline T vk_check_value(vk::ResultValue<T> rv, const char* what)
    {
        vk_check(rv.result, what);
        return rv.value;
    }
}

#ifdef VULKAN_HPP_NO_EXCEPTIONS
    // Use for vk::Result-returning calls.
    #define VK_CHECK(expr) ::vkmini::vk_check((expr), #expr)
    // Use for vk::ResultValue<T>-returning calls.
    #define VK_CHECK_VALUE(expr) ::vkmini::vk_check_value((expr), #expr)
#else
    // Exceptions enabled: Vulkan-Hpp throws on failure.
    #define VK_CHECK(expr) (void)(expr)
    #define VK_CHECK_VALUE(expr) (expr)
#endif
