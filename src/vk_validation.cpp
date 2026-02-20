#include "vk_validation.hpp"
#include <iostream>
#include <cstring>

namespace vkmini {

VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    std::cerr << "validation: " << (data && data->pMessage ? data->pMessage : "(null)") << "\n";
    return VK_FALSE;
}

ValidationConfig make_validation_config()
{
    ValidationConfig cfg{};
#if VKMINI_ENABLE_VALIDATION
    cfg.enable = true;
    // runtime check: only enable if available
    const auto available = vk::enumerateInstanceLayerProperties();
    bool has = false;
    for (const auto& p : available)
        if (std::strcmp(p.layerName, "VK_LAYER_KHRONOS_validation") == 0) { has = true; break; }

    if (has)
    {
        cfg.instance_layers.push_back("VK_LAYER_KHRONOS_validation");
        cfg.instance_exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    else
    {
        cfg.enable = false;
        std::cerr << "[Vulkan] VK_LAYER_KHRONOS_validation not found; running without validation.\n";
    }
#else
    cfg.enable = false;
#endif
    return cfg;
}

DebugMessenger create_debug_messenger(vk::Instance instance)
{
    DebugMessenger out{};
#if VKMINI_ENABLE_VALIDATION
    auto fpCreate = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    out.destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

    if (!fpCreate || !out.destroy) return out;

    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    ci.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = debug_callback;

    (void)fpCreate(instance, &ci, nullptr, &out.handle);
#endif
    return out;
}

} // namespace vkmini
