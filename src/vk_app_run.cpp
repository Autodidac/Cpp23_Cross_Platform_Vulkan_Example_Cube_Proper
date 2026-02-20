#include "vk_state.hpp"
#include "vk_internal.hpp"
#include "vk_helpers.hpp"
#include "vk_check.hpp"
#include "vk_validation.hpp"
#include "math.hpp"
#include "platform.hpp"

#include <chrono>
#include <array>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>

namespace vkmini {

static void recreate_swapchain(AppState& s, IPlatformWindow& wnd)
{
    s.device->waitIdle();

    // destroy swapchain-dependent
    s.cmdBuffers.clear();
    s.pipe.framebuffers.clear();
    s.pipe.pipeline.reset();
    s.pipe.pipelineLayout.reset();
    s.pipe.renderPass.reset();
    s.depth.view.reset();
    s.depth.img.reset();
    s.depth.mem.reset();
    s.sc.views.clear();
    s.sc.images.clear();
    s.sc.swapchain.reset();

    // rebuild (reuse helper from setup unit via duplicated minimal logic)
    // To keep files independent and <800 lines, we just inline the essentials here.
    while (wnd.is_minimized())
        wnd.wait_events();

    const auto fb = wnd.framebuffer_size();

    const auto caps = s.pd.getSurfaceCapabilitiesKHR(s.surface.get());
    const auto formats = s.pd.getSurfaceFormatsKHR(s.surface.get());
    const auto modes = s.pd.getSurfacePresentModesKHR(s.surface.get());

    s.sc.surfFmt = pick_surface_format(formats);
    s.sc.presentMode = pick_present_mode(modes);

    vk::Extent2D extent{};
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        extent = caps.currentExtent;
    else
        extent = vk::Extent2D{
            std::clamp(fb.w, caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp(fb.h, caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    s.sc.extent = extent;

    uint32_t minCount = std::max(2u, caps.minImageCount);
    uint32_t maxCount = (caps.maxImageCount == 0) ? minCount : caps.maxImageCount;
    uint32_t imageCount = std::min(minCount, maxCount);

    std::array<uint32_t,2> families = { s.graphicsQ, s.presentQ };
    const bool concurrent = s.graphicsQ != s.presentQ;

    vk::SwapchainCreateInfoKHR sci(
        {}, s.surface.get(), imageCount,
        s.sc.surfFmt.format, s.sc.surfFmt.colorSpace, s.sc.extent,
        1, vk::ImageUsageFlagBits::eColorAttachment,
        concurrent ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
        concurrent ? (uint32_t)families.size() : 0u,
        concurrent ? families.data() : nullptr,
        caps.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        s.sc.presentMode,
        true,
        vk::SwapchainKHR{}
    );

    s.sc.swapchain = s.device->createSwapchainKHRUnique(sci);
    s.sc.images = s.device->getSwapchainImagesKHR(s.sc.swapchain.get());

    s.sc.views.reserve(s.sc.images.size());
    for (auto img : s.sc.images)
    {
        s.sc.views.push_back(s.device->createImageViewUnique(vk::ImageViewCreateInfo{
            {}, img, vk::ImageViewType::e2D, s.sc.surfFmt.format,
            {}, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0,1,0,1 }
        }));
    }

    // depth
    s.depth.depthFmt = find_depth_format(s.pd);
    {
        auto img = create_image(s.pd, s.device.get(), s.sc.extent.width, s.sc.extent.height,
            s.depth.depthFmt, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        s.depth.img = std::move(img.img);
        s.depth.mem = std::move(img.mem);
        s.depth.view = s.device->createImageViewUnique(vk::ImageViewCreateInfo{
            {}, s.depth.img.get(), vk::ImageViewType::e2D, s.depth.depthFmt,
            {}, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0,1,0,1 }
        });
    }

    // renderpass
    {
        const vk::AttachmentDescription colorAtt(
            {}, s.sc.surfFmt.format, vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);

        const vk::AttachmentDescription depthAtt(
            {}, s.depth.depthFmt, vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

        std::array<vk::AttachmentDescription,2> atts = { colorAtt, depthAtt };
        vk::AttachmentReference colorRef(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::AttachmentReference depthRef(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

        vk::SubpassDescription subpass(
            {}, vk::PipelineBindPoint::eGraphics,
            0,nullptr,
            1,&colorRef,
            nullptr,
            &depthRef);

        vk::SubpassDependency dep(
            VK_SUBPASS_EXTERNAL, 0,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            {}, vk::AccessFlagBits::eColorAttachmentWrite);

        s.pipe.renderPass = s.device->createRenderPassUnique(vk::RenderPassCreateInfo{
            {}, (uint32_t)atts.size(), atts.data(), 1, &subpass, 1, &dep
        });
    }

    // NOTE: pipeline is created in setup unit; to keep this file standalone, we just keep pipeline fixed by reusing the same shaders?
    // For simplicity in this sample, we rebuild pipeline by calling into setup file's compilation would duplicate shaderc;
    // Instead, accept that initial pipeline exists? Not safe. So we mark "needs pipeline rebuild" by throwing and letting user restart.
    // BUT user demanded resize safety; so we need pipeline rebuild here too. We'll do minimal fixed-function pipeline with no shaderc by using precompiled? not available.
    // Therefore we keep the pipeline creation in setup and include a tiny helper via extern.
}


static void recreate_swapchain_full(AppState& s, IPlatformWindow& wnd)
{
    recreate_swapchain(s, wnd);
    // recreate_swapchain() rebuilt renderpass+depth+views; now rebuild pipeline+fb+cmd via helpers
    setup_pipeline(s);
    create_framebuffers(s);
    create_cmd_buffers(s);
    s.sync.imagesInFlight.assign(s.sc.images.size(), vk::Fence{});
}

void run_loop(AppState& s, IPlatformWindow& wnd)
{
    // Debug messenger lifetime: create/destroy around run
    DebugMessenger dbg = create_debug_messenger(s.instance.get());

    const auto t0 = std::chrono::high_resolution_clock::now();

    while (wnd.pump_events())
    {
        if (wnd.is_minimized())
        {
            wnd.wait_events();
            continue;
        }

        const auto frame = s.sync.frameIndex;

        VK_CHECK(s.device->waitForFences(s.sync.frameFence[frame].get(), true, std::numeric_limits<uint64_t>::max()));

        // Acquire (must tolerate resize / minimize)
        uint32_t imageIndex = 0;
        try
        {
#ifdef VULKAN_HPP_NO_EXCEPTIONS
            vk::ResultValue<uint32_t> acquire = s.device->acquireNextImageKHR(
                s.sc.swapchain.get(),
                std::numeric_limits<uint64_t>::max(),
                s.sync.imageAvailable[frame].get(),
                nullptr
            );

            if (acquire.result == vk::Result::eErrorOutOfDateKHR)
            {
                recreate_swapchain_full(s, wnd);
                continue;
            }
            if (acquire.result != vk::Result::eSuccess && acquire.result != vk::Result::eSuboptimalKHR)
                continue;

            imageIndex = acquire.value;
#else
            // Exceptions enabled: throws vk::OutOfDateKHRError on resize.
            imageIndex = s.device->acquireNextImageKHR(
                s.sc.swapchain.get(),
                std::numeric_limits<uint64_t>::max(),
                s.sync.imageAvailable[frame].get(),
                nullptr
            ).value;
#endif
        }
        catch (const vk::OutOfDateKHRError&)
        {
            recreate_swapchain_full(s, wnd);
            continue;
        }

        if (s.sync.imagesInFlight[imageIndex])
            VK_CHECK(s.device->waitForFences(s.sync.imagesInFlight[imageIndex], true, std::numeric_limits<uint64_t>::max()));

        s.sync.imagesInFlight[imageIndex] = s.sync.frameFence[frame].get();

        VK_CHECK(s.device->resetFences(s.sync.frameFence[frame].get()));

        // UBO update
        const auto t1 = std::chrono::high_resolution_clock::now();
        const float seconds = std::chrono::duration<float>(t1 - t0).count();

        const float aspect = (float)s.sc.extent.width / (float)s.sc.extent.height;
        Mat4 proj = perspective(45.0f * 3.1415926f / 180.0f, aspect, 0.1f, 100.0f);
        proj.m[5] *= -1.0f; // Vulkan Y flip

        Mat4 view = translate(0.0f, 0.0f, -4.0f);
        Mat4 model = mul(rotate_y(seconds), rotate_x(seconds * 0.7f));
        Mat4 mvp = mul(proj, mul(view, model));

        UBO u{};
        std::memcpy(u.mvp, mvp.m.data(), sizeof(u.mvp));
        void* umap = s.device->mapMemory(s.ubo.mem.get(), 0, sizeof(UBO));
        std::memcpy(umap, &u, sizeof(UBO));
        s.device->unmapMemory(s.ubo.mem.get());

        // Record CB
        auto& cb = s.cmdBuffers[imageIndex];
        cb->reset();
        cb->begin(vk::CommandBufferBeginInfo{});

        std::array<vk::ClearValue,2> clears{};
        clears[0].color = vk::ClearColorValue(std::array<float,4>{0.05f,0.05f,0.08f,1.0f});
        clears[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

        vk::RenderPassBeginInfo rpbi{
            s.pipe.renderPass.get(),
            s.pipe.framebuffers[imageIndex].get(),
            vk::Rect2D{{0,0}, s.sc.extent},
            (uint32_t)clears.size(), clears.data()
        };

        cb->beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb->bindPipeline(vk::PipelineBindPoint::eGraphics, s.pipe.pipeline.get());

        vk::DeviceSize offs[] = {0};
        vk::Buffer vb = s.vbo.buf.get();
        cb->bindVertexBuffers(0, 1, &vb, offs);

        vk::DescriptorSet ds = s.dset.get();
        cb->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, s.pipe.pipelineLayout.get(), 0, 1, &ds, 0, nullptr);

        cb->draw(36, 1, 0, 0);
        cb->endRenderPass();
        cb->end();

        // Submit
        const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::Semaphore ia = s.sync.imageAvailable[frame].get();
        vk::Semaphore rf = s.sync.renderFinished[frame].get();
        vk::CommandBuffer cbh = cb.get();

        vk::SubmitInfo submit{ 1, &ia, &waitStage, 1, &cbh, 1, &rf };
        s.graphicsQueue.submit(submit, s.sync.frameFence[frame].get());

        // Present
        vk::SwapchainKHR sc = s.sc.swapchain.get();
        vk::PresentInfoKHR present{ 1, &rf, 1, &sc, &imageIndex };
        try
        {
            const vk::Result pres = s.presentQueue.presentKHR(present);
            if (pres == vk::Result::eErrorOutOfDateKHR || pres == vk::Result::eSuboptimalKHR)
                recreate_swapchain_full(s, wnd);
        }
        catch (const vk::OutOfDateKHRError&)
        {
            recreate_swapchain_full(s, wnd);
        }

        s.sync.frameIndex = (s.sync.frameIndex + 1) % SyncState::kMaxFramesInFlight;
    }

    s.device->waitIdle();

    if (dbg.destroy && dbg.handle)
        dbg.destroy(s.instance.get(), dbg.handle, nullptr);
}

} // namespace vkmini
