#pragma once

#include "vk_platform.hpp" // must be before Vulkan-Hpp
#include <vulkan/vulkan.hpp>
#include <vector>
#include <array>
#include <cstdint>

namespace vkmini {

struct Vertex { float px,py,pz; float u,v; };
struct UBO { float mvp[16]; };

struct SwapchainState {
    vk::UniqueSwapchainKHR swapchain;
    vk::SurfaceFormatKHR   surfFmt{};
    vk::PresentModeKHR     presentMode{};
    vk::Extent2D           extent{};
    std::vector<vk::Image> images;
    std::vector<vk::UniqueImageView> views;
};

struct SyncState {
    static constexpr uint32_t kMaxFramesInFlight = 2;
    std::array<vk::UniqueSemaphore, kMaxFramesInFlight> imageAvailable{};
    std::array<vk::UniqueSemaphore, kMaxFramesInFlight> renderFinished{};
    std::array<vk::UniqueFence,     kMaxFramesInFlight> frameFence{};
    std::vector<vk::Fence> imagesInFlight;
    uint32_t frameIndex = 0;
};

struct PipelineState {
    vk::UniqueRenderPass renderPass;
    vk::UniquePipelineLayout pipelineLayout;
    vk::UniquePipeline pipeline;
    std::vector<vk::UniqueFramebuffer> framebuffers;
};

struct DepthState {
    vk::Format depthFmt{};
    vk::UniqueImageView view;
    vk::UniqueImage img;
    vk::UniqueDeviceMemory mem;
};

struct TextureState {
    vk::UniqueImage img;
    vk::UniqueDeviceMemory mem;
    vk::UniqueImageView view;
    vk::UniqueSampler sampler;
};

struct BufferState {
    vk::UniqueBuffer buf;
    vk::UniqueDeviceMemory mem;
};

struct AppState {
    vk::UniqueInstance instance;
    vk::UniqueDevice device;
    vk::PhysicalDevice pd{};
    vk::UniqueSurfaceKHR surface;

    vk::Queue graphicsQueue;
    vk::Queue presentQueue;
    uint32_t graphicsQ = 0;
    uint32_t presentQ = 0;

    vk::UniqueCommandPool cmdPool;
    std::vector<vk::UniqueCommandBuffer> cmdBuffers;

    SwapchainState sc;
    DepthState depth;
    PipelineState pipe;
    SyncState sync;

    TextureState tex;
    BufferState vbo;
    BufferState ubo;

    vk::UniqueDescriptorSetLayout dsl;
    vk::UniqueDescriptorPool dpool;
    vk::UniqueDescriptorSet dset;
};

} // namespace vkmini
