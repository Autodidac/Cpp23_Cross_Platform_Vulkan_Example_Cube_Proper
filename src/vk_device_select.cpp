#include "vk_device_select.hpp"
#include "vk_helpers.hpp"
#include <cstring>
#include <stdexcept>

namespace vkmini {

static bool supports_swapchain(vk::PhysicalDevice pd)
{
    for (const auto& e : pd.enumerateDeviceExtensionProperties())
        if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            return true;
    return false;
}

vk::PhysicalDevice pick_best_device(const std::vector<vk::PhysicalDevice>& devices, vk::SurfaceKHR surface)
{
    vk::PhysicalDevice best{};
    int bestScore = -1;

    for (auto pd : devices)
    {
        if (!supports_swapchain(pd)) continue;

        // Require graphics + present queue families.
        const auto qfps = pd.getQueueFamilyProperties();
        bool hasG=false, hasP=false;
        for (uint32_t i=0;i<(uint32_t)qfps.size();++i)
        {
            if (qfps[i].queueFlags & vk::QueueFlagBits::eGraphics) hasG=true;
            if (pd.getSurfaceSupportKHR(i, surface)) hasP=true;
        }
        if (!hasG || !hasP) continue;

        const auto props = pd.getProperties();
        int score = 0;
        if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 1000;
        // VRAM heuristic
        const auto mem = pd.getMemoryProperties();
        for (uint32_t h=0; h<mem.memoryHeapCount; ++h)
            if (mem.memoryHeaps[h].flags & vk::MemoryHeapFlagBits::eDeviceLocal)
                score += int(mem.memoryHeaps[h].size / (256ull*1024ull*1024ull));

        if (score > bestScore) { bestScore = score; best = pd; }
    }

    if (!best) throw std::runtime_error("No suitable Vulkan device found.");
    return best;
}

} // namespace vkmini
