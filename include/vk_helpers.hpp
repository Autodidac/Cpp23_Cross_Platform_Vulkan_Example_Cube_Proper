#pragma once
#include <vulkan/vulkan.hpp>
#include <cstdint>
#include <vector>

namespace vkmini {

uint32_t pick_graphics_qf(vk::PhysicalDevice pd);
uint32_t pick_present_qf(vk::PhysicalDevice pd, vk::SurfaceKHR surface);

vk::SurfaceFormatKHR pick_surface_format(const std::vector<vk::SurfaceFormatKHR>& formats);
vk::PresentModeKHR pick_present_mode(const std::vector<vk::PresentModeKHR>& modes);
vk::Format find_depth_format(vk::PhysicalDevice pd);

struct Buffer {
    vk::UniqueBuffer buf;
    vk::UniqueDeviceMemory mem;
};

struct Image {
    vk::UniqueImage img;
    vk::UniqueDeviceMemory mem;
};

Buffer create_buffer(vk::PhysicalDevice pd, vk::Device dev, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags props);
Image  create_image(vk::PhysicalDevice pd, vk::Device dev, uint32_t w, uint32_t h, vk::Format fmt, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags props);

void copy_buffer(vk::Device dev, vk::Queue q, vk::CommandPool pool, vk::Buffer src, vk::Buffer dst, vk::DeviceSize size);
void transition_image_layout(vk::Device dev, vk::Queue q, vk::CommandPool pool, vk::Image image, vk::Format fmt, vk::ImageLayout oldL, vk::ImageLayout newL);
void copy_buffer_to_image(vk::Device dev, vk::Queue q, vk::CommandPool pool, vk::Buffer src, vk::Image dst, uint32_t w, uint32_t h);

} // namespace vkmini
