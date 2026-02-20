#pragma once
#include <vulkan/vulkan.hpp>
#include <vector>

namespace vkmini {

struct ValidationConfig {
    bool enable = true;
    std::vector<const char*> instance_layers;
    std::vector<const char*> instance_exts;
};

ValidationConfig make_validation_config();
VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT*,
    void*);

struct DebugMessenger {
    VkDebugUtilsMessengerEXT handle = VK_NULL_HANDLE;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy = nullptr;
};

DebugMessenger create_debug_messenger(vk::Instance instance);

} // namespace vkmini
