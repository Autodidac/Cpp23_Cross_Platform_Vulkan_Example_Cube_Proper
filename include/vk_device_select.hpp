#pragma once
#include <vulkan/vulkan.hpp>
#include <vector>

namespace vkmini {

// Chooses a "best" device for the given surface.
// Security note: device selection is *policy*; ship games should typically pin a known-good device or verify driver versions.
vk::PhysicalDevice pick_best_device(const std::vector<vk::PhysicalDevice>& devices, vk::SurfaceKHR surface);

} // namespace vkmini
