#include "vk_helpers.hpp"
#include <stdexcept>
#include <cstring>

namespace vkmini {

static uint32_t find_mem_type(vk::PhysicalDevice pd, uint32_t typeFilter, vk::MemoryPropertyFlags props)
{
    auto mem = pd.getMemoryProperties();
    for (uint32_t i=0;i<mem.memoryTypeCount;++i)
        if ((typeFilter & (1u<<i)) && ((mem.memoryTypes[i].propertyFlags & props) == props))
            return i;
    throw std::runtime_error("No suitable memory type");
}

uint32_t pick_graphics_qf(vk::PhysicalDevice pd)
{
    auto qfps = pd.getQueueFamilyProperties();
    for (uint32_t i=0;i<(uint32_t)qfps.size();++i)
        if (qfps[i].queueFlags & vk::QueueFlagBits::eGraphics) return i;
    throw std::runtime_error("No graphics queue family");
}

uint32_t pick_present_qf(vk::PhysicalDevice pd, vk::SurfaceKHR surface)
{
    auto qfps = pd.getQueueFamilyProperties();
    for (uint32_t i=0;i<(uint32_t)qfps.size();++i)
        if (pd.getSurfaceSupportKHR(i, surface)) return i;
    throw std::runtime_error("No present queue family");
}

vk::SurfaceFormatKHR pick_surface_format(const std::vector<vk::SurfaceFormatKHR>& formats)
{
    for (auto& f : formats)
        if (f.format == vk::Format::eB8G8R8A8Srgb && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return f;
    return formats.empty() ? vk::SurfaceFormatKHR{} : formats[0];
}

vk::PresentModeKHR pick_present_mode(const std::vector<vk::PresentModeKHR>& modes)
{
    for (auto m : modes) if (m == vk::PresentModeKHR::eMailbox) return m;
    for (auto m : modes) if (m == vk::PresentModeKHR::eFifo) return m;
    return modes.empty() ? vk::PresentModeKHR::eFifo : modes[0];
}

vk::Format find_depth_format(vk::PhysicalDevice pd)
{
    const vk::Format candidates[] = { vk::Format::eD32Sfloat, vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint };
    for (auto fmt : candidates)
    {
        auto props = pd.getFormatProperties(fmt);
        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
            return fmt;
    }
    return vk::Format::eD32Sfloat;
}

Buffer create_buffer(vk::PhysicalDevice pd, vk::Device dev, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags props)
{
    Buffer out{};
    out.buf = dev.createBufferUnique(vk::BufferCreateInfo{ {}, size, usage, vk::SharingMode::eExclusive });

    auto req = dev.getBufferMemoryRequirements(out.buf.get());
    out.mem = dev.allocateMemoryUnique(vk::MemoryAllocateInfo{ req.size, find_mem_type(pd, req.memoryTypeBits, props) });
    dev.bindBufferMemory(out.buf.get(), out.mem.get(), 0);
    return out;
}

Image create_image(vk::PhysicalDevice pd, vk::Device dev, uint32_t w, uint32_t h, vk::Format fmt, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags props)
{
    Image out{};
    out.img = dev.createImageUnique(vk::ImageCreateInfo{
        {}, vk::ImageType::e2D, fmt,
        vk::Extent3D{w,h,1},
        1,1, vk::SampleCountFlagBits::e1,
        tiling, usage
    });
    auto req = dev.getImageMemoryRequirements(out.img.get());
    out.mem = dev.allocateMemoryUnique(vk::MemoryAllocateInfo{ req.size, find_mem_type(pd, req.memoryTypeBits, props) });
    dev.bindImageMemory(out.img.get(), out.mem.get(), 0);
    return out;
}

static vk::UniqueCommandBuffer begin_one_time(vk::Device dev, vk::CommandPool pool)
{
    auto bufs = dev.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{ pool, vk::CommandBufferLevel::ePrimary, 1 });
    auto cb = std::move(bufs[0]);
    cb->begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    return cb;
}

static void end_one_time(vk::Device dev, vk::Queue q, vk::CommandPool pool, vk::UniqueCommandBuffer& cb)
{
    cb->end();
    vk::SubmitInfo si{ 0,nullptr,nullptr, 1,&cb.get(), 0,nullptr };
    q.submit(si, vk::Fence{});
    q.waitIdle();
    (void)dev;
    (void)pool;
    // IMPORTANT:
    // Do NOT call vkFreeCommandBuffers here.
    // vk::UniqueCommandBuffer owns the handle and frees it in its destructor.
}

void copy_buffer(vk::Device dev, vk::Queue q, vk::CommandPool pool, vk::Buffer src, vk::Buffer dst, vk::DeviceSize size)
{
    auto cb = begin_one_time(dev, pool);
    cb->copyBuffer(src, dst, vk::BufferCopy{0,0,size});
    end_one_time(dev, q, pool, cb);
}

void transition_image_layout(vk::Device dev, vk::Queue q, vk::CommandPool pool, vk::Image image, vk::Format, vk::ImageLayout oldL, vk::ImageLayout newL)
{
    auto cb = begin_one_time(dev, pool);

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = oldL;
    barrier.newLayout = newL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0,1,0,1 };

    vk::PipelineStageFlags srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
    vk::PipelineStageFlags dstStage = vk::PipelineStageFlagBits::eTransfer;

    if (oldL == vk::ImageLayout::eTransferDstOptimal && newL == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        srcStage = vk::PipelineStageFlagBits::eTransfer;
        dstStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else if (oldL == vk::ImageLayout::eUndefined && newL == vk::ImageLayout::eTransferDstOptimal)
    {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
        dstStage = vk::PipelineStageFlagBits::eTransfer;
    }

    cb->pipelineBarrier(srcStage, dstStage, {}, 0,nullptr, 0,nullptr, 1,&barrier);
    end_one_time(dev, q, pool, cb);
}

void copy_buffer_to_image(vk::Device dev, vk::Queue q, vk::CommandPool pool, vk::Buffer src, vk::Image dst, uint32_t w, uint32_t h)
{
    auto cb = begin_one_time(dev, pool);
    vk::BufferImageCopy region{};
    region.imageSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0,0,1 };
    region.imageExtent = vk::Extent3D{w,h,1};
    cb->copyBufferToImage(src, dst, vk::ImageLayout::eTransferDstOptimal, 1, &region);
    end_one_time(dev, q, pool, cb);
}

} // namespace vkmini
