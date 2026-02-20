#include "vk_state.hpp"
#include "vk_internal.hpp"
#include "vk_validation.hpp"
#include "vk_device_select.hpp"
#include "vk_helpers.hpp"
#include "vk_check.hpp"
#include "platform.hpp"

#include <shaderc/shaderc.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace vkmini {

static constexpr std::array<Vertex, 36> kCube = {{
    {-1,-1, 1, 0,1}, { 1,-1, 1, 1,1}, { 1, 1, 1, 1,0},
    {-1,-1, 1, 0,1}, { 1, 1, 1, 1,0}, {-1, 1, 1, 0,0},
    { 1,-1,-1, 0,1}, {-1,-1,-1, 1,1}, {-1, 1,-1, 1,0},
    { 1,-1,-1, 0,1}, {-1, 1,-1, 1,0}, { 1, 1,-1, 0,0},
    {-1,-1,-1, 0,1}, {-1,-1, 1, 1,1}, {-1, 1, 1, 1,0},
    {-1,-1,-1, 0,1}, {-1, 1, 1, 1,0}, {-1, 1,-1, 0,0},
    { 1,-1, 1, 0,1}, { 1,-1,-1, 1,1}, { 1, 1,-1, 1,0},
    { 1,-1, 1, 0,1}, { 1, 1,-1, 1,0}, { 1, 1, 1, 0,0},
    {-1, 1, 1, 0,1}, { 1, 1, 1, 1,1}, { 1, 1,-1, 1,0},
    {-1, 1, 1, 0,1}, { 1, 1,-1, 1,0}, {-1, 1,-1, 0,0},
    {-1,-1,-1, 0,1}, { 1,-1,-1, 1,1}, { 1,-1, 1, 1,0},
    {-1,-1,-1, 0,1}, { 1,-1, 1, 1,0}, {-1,-1, 1, 0,0},
}};

static std::vector<uint32_t> compile_glsl_to_spv(const std::string& src, shaderc_shader_kind kind, const char* name)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions opts;
    opts.SetOptimizationLevel(shaderc_optimization_level_performance);
    auto result = compiler.CompileGlslToSpv(src, kind, name, opts);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        throw std::runtime_error(std::string("shaderc failed: ") + result.GetErrorMessage());
    return { result.cbegin(), result.cend() };
}

static void create_swapchain(AppState& s, IPlatformWindow& wnd);
static void destroy_swapchain_deps(AppState& s);

static void setup_instance(AppState& s)
{
    vk::ApplicationInfo appInfo("vkmini", VK_MAKE_VERSION(1,0,0), "none", VK_MAKE_VERSION(1,0,0), VK_API_VERSION_1_2);

    auto vcfg = make_validation_config();

    std::vector<const char*> exts;
    exts.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(_WIN32)
    exts.push_back("VK_KHR_win32_surface");
#elif defined(__ANDROID__)
    exts.push_back("VK_KHR_android_surface");
#else
    exts.push_back("VK_KHR_xcb_surface");
#endif

    for (auto e : vcfg.instance_exts) exts.push_back(e);

    vk::InstanceCreateInfo ici{};
    ici.pApplicationInfo = &appInfo;
    ici.enabledLayerCount = (uint32_t)vcfg.instance_layers.size();
    ici.ppEnabledLayerNames = vcfg.instance_layers.data();
    ici.enabledExtensionCount = (uint32_t)exts.size();
    ici.ppEnabledExtensionNames = exts.data();

    s.instance = vk::createInstanceUnique(ici);
}

static void setup_surface(AppState& s, IPlatformWindow& wnd)
{
    auto n = wnd.native();
#if defined(_WIN32)
    s.surface = s.instance->createWin32SurfaceKHRUnique(vk::Win32SurfaceCreateInfoKHR{ {}, (HINSTANCE)n.hinstance, (HWND)n.hwnd });
#elif defined(__ANDROID__)
    if (!n.android_native_window)
        throw std::runtime_error("Android native window is null.");
    s.surface = s.instance->createAndroidSurfaceKHRUnique(
        vk::AndroidSurfaceCreateInfoKHR{ {}, (ANativeWindow*)n.android_native_window });
#else
    s.surface = s.instance->createXcbSurfaceKHRUnique(vk::XcbSurfaceCreateInfoKHR{ {}, (xcb_connection_t*)n.xcb_connection, (xcb_window_t)n.xcb_window });
#endif
}

static void setup_device(AppState& s)
{
    auto devices = s.instance->enumeratePhysicalDevices();
    if (devices.empty()) throw std::runtime_error("No Vulkan physical devices");

    s.pd = pick_best_device(devices, s.surface.get());

    const auto props = s.pd.getProperties();
    std::cout << "[Vulkan] Using: " << props.deviceName << " (vendor 0x" << std::hex << props.vendorID << std::dec << ")\n";

    s.graphicsQ = pick_graphics_qf(s.pd);
    s.presentQ  = pick_present_qf(s.pd, s.surface.get());

    std::set<uint32_t> unique = { s.graphicsQ, s.presentQ };
    float prio = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> qcis;
    for (auto qf : unique) qcis.push_back(vk::DeviceQueueCreateInfo{ {}, qf, 1, &prio });

    std::vector<const char*> devExts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    vk::DeviceCreateInfo dci{};
    dci.queueCreateInfoCount = (uint32_t)qcis.size();
    dci.pQueueCreateInfos = qcis.data();
    dci.enabledExtensionCount = (uint32_t)devExts.size();
    dci.ppEnabledExtensionNames = devExts.data();

    s.device = s.pd.createDeviceUnique(dci);
    s.graphicsQueue = s.device->getQueue(s.graphicsQ, 0);
    s.presentQueue  = s.device->getQueue(s.presentQ, 0);

    s.cmdPool = s.device->createCommandPoolUnique(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQ
    });
}

static void setup_assets(AppState& s)
{
    // texture: simple checkerboard RGBA8
    const uint32_t texW=256, texH=256;
    std::vector<uint32_t> pixels(texW*texH);
    for (uint32_t y=0;y<texH;++y)
    for (uint32_t x=0;x<texW;++x)
    {
        const bool on = (((x/32) ^ (y/32)) & 1) != 0;
        const uint8_t c = on ? 255 : 32;
        pixels[y*texW + x] = (uint32_t)c | ((uint32_t)c<<8) | ((uint32_t)c<<16) | (0xFFu<<24);
    }

    const vk::DeviceSize bytes = pixels.size()*sizeof(uint32_t);

    auto staging = create_buffer(s.pd, s.device.get(), bytes,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* map = s.device->mapMemory(staging.mem.get(), 0, bytes);
    std::memcpy(map, pixels.data(), (size_t)bytes);
    s.device->unmapMemory(staging.mem.get());

    auto img = create_image(s.pd, s.device.get(), texW, texH,
        vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    transition_image_layout(s.device.get(), s.graphicsQueue, s.cmdPool.get(),
        img.img.get(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    copy_buffer_to_image(s.device.get(), s.graphicsQueue, s.cmdPool.get(), staging.buf.get(), img.img.get(), texW, texH);

    transition_image_layout(s.device.get(), s.graphicsQueue, s.cmdPool.get(),
        img.img.get(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    s.tex.img = std::move(img.img);
    s.tex.mem = std::move(img.mem);

    s.tex.view = s.device->createImageViewUnique(vk::ImageViewCreateInfo{
        {}, s.tex.img.get(), vk::ImageViewType::e2D, vk::Format::eR8G8B8A8Unorm,
        {}, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0,1,0,1 }
    });

    s.tex.sampler = s.device->createSamplerUnique(vk::SamplerCreateInfo{
        {},
        vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat
    });

    // VBO + UBO
    const vk::DeviceSize vboBytes = sizeof(Vertex) * kCube.size();

    auto vboStage = create_buffer(s.pd, s.device.get(), vboBytes,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* vmap = s.device->mapMemory(vboStage.mem.get(), 0, vboBytes);
    std::memcpy(vmap, kCube.data(), (size_t)vboBytes);
    s.device->unmapMemory(vboStage.mem.get());

    auto vbo = create_buffer(s.pd, s.device.get(), vboBytes,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    copy_buffer(s.device.get(), s.graphicsQueue, s.cmdPool.get(), vboStage.buf.get(), vbo.buf.get(), vboBytes);

    s.vbo.buf = std::move(vbo.buf);
    s.vbo.mem = std::move(vbo.mem);

    auto ubo = create_buffer(s.pd, s.device.get(), sizeof(UBO),
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    s.ubo.buf = std::move(ubo.buf);
    s.ubo.mem = std::move(ubo.mem);

    // descriptors
    std::array<vk::DescriptorSetLayoutBinding,2> bindings = {
        vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },
        vk::DescriptorSetLayoutBinding{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment }
    };

    s.dsl = s.device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
        {}, (uint32_t)bindings.size(), bindings.data()
    });

    std::array<vk::DescriptorPoolSize,2> sizes = {
        vk::DescriptorPoolSize{ vk::DescriptorType::eUniformBuffer, 1 },
        vk::DescriptorPoolSize{ vk::DescriptorType::eCombinedImageSampler, 1 }
    };

    s.dpool = s.device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
        {}, 1, (uint32_t)sizes.size(), sizes.data()
    });

    s.dset = std::move(s.device->allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo{
        s.dpool.get(), 1, &s.dsl.get()
    })[0]);

    vk::DescriptorBufferInfo dbi{ s.ubo.buf.get(), 0, sizeof(UBO) };
    vk::DescriptorImageInfo dii{ s.tex.sampler.get(), s.tex.view.get(), vk::ImageLayout::eShaderReadOnlyOptimal };

    std::array<vk::WriteDescriptorSet,2> writes = {
        vk::WriteDescriptorSet{ s.dset.get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &dbi, nullptr },
        vk::WriteDescriptorSet{ s.dset.get(), 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &dii, nullptr, nullptr }
    };
    s.device->updateDescriptorSets((uint32_t)writes.size(), writes.data(), 0, nullptr);
}

void setup_pipeline(AppState& s)
{
    // Renderpass created in swapchain build.
    // Pipeline uses current swapchain extent.
    const std::string vertSrc = R"glsl(
        #version 450
        layout(location=0) in vec3 inPos;
        layout(location=1) in vec2 inUV;
        layout(binding=0) uniform UBO { mat4 mvp; } ubo;
        layout(location=0) out vec2 vUV;
        void main() {
            gl_Position = ubo.mvp * vec4(inPos, 1.0);
            vUV = inUV;
        }
    )glsl";

    const std::string fragSrc = R"glsl(
        #version 450
        layout(location=0) in vec2 vUV;
        layout(location=0) out vec4 outColor;
        layout(binding=1) uniform sampler2D texSampler;
        void main() {
            outColor = texture(texSampler, vUV);
        }
    )glsl";

    auto vertSpv = compile_glsl_to_spv(vertSrc, shaderc_glsl_vertex_shader, "vert");
    auto fragSpv = compile_glsl_to_spv(fragSrc, shaderc_glsl_fragment_shader, "frag");

    auto vert = s.device->createShaderModuleUnique(vk::ShaderModuleCreateInfo{ {}, vertSpv.size()*4, vertSpv.data() });
    auto frag = s.device->createShaderModuleUnique(vk::ShaderModuleCreateInfo{ {}, fragSpv.size()*4, fragSpv.data() });

    vk::PipelineShaderStageCreateInfo stages[2] = {
        vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, vert.get(), "main" },
        vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, frag.get(), "main" }
    };

    vk::VertexInputBindingDescription bind{ 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    std::array<vk::VertexInputAttributeDescription,2> attr = {
        vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, px) },
        vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, u) }
    };

    vk::PipelineVertexInputStateCreateInfo vi{ {}, 1, &bind, (uint32_t)attr.size(), attr.data() };
    vk::PipelineInputAssemblyStateCreateInfo ia{ {}, vk::PrimitiveTopology::eTriangleList, false };

    vk::Viewport vp{ 0,0, (float)s.sc.extent.width, (float)s.sc.extent.height, 0,1 };
    vk::Rect2D sc{ {0,0}, s.sc.extent };
    vk::PipelineViewportStateCreateInfo vpState{ {}, 1, &vp, 1, &sc };

    // IMPORTANT: we flip Y in projection; that flips winding.
    // Keep backface culling, but treat clockwise as front.
    vk::PipelineRasterizationStateCreateInfo rs{
        {}, false, false,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eFront,
        vk::FrontFace::eClockwise,
        false,0,0,0, 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo ms{ {}, vk::SampleCountFlagBits::e1, false };
    vk::PipelineDepthStencilStateCreateInfo ds{ {}, true, true, vk::CompareOp::eLess, false, false };

    vk::PipelineColorBlendAttachmentState ba{
        false,
        vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };
    vk::PipelineColorBlendStateCreateInfo cb{ {}, false, vk::LogicOp::eCopy, 1, &ba };

    s.pipe.pipelineLayout = s.device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{
        {}, 1, &s.dsl.get(), 0, nullptr
    });

    vk::GraphicsPipelineCreateInfo gpi{
        {}, 2, stages,
        &vi, &ia,
        nullptr,
        &vpState,
        &rs, &ms, &ds, &cb,
        nullptr,
        s.pipe.pipelineLayout.get(),
        s.pipe.renderPass.get(),
        0
    };

    s.pipe.pipeline = s.device->createGraphicsPipelineUnique({}, gpi).value;
}

static void setup_sync(AppState& s)
{
    using S = SyncState;
    for (uint32_t i=0;i<S::kMaxFramesInFlight;++i)
    {
        s.sync.imageAvailable[i] = s.device->createSemaphoreUnique(vk::SemaphoreCreateInfo{});
        s.sync.renderFinished[i] = s.device->createSemaphoreUnique(vk::SemaphoreCreateInfo{});
        s.sync.frameFence[i] = s.device->createFenceUnique(vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled });
    }
}

static void create_depth(AppState& s)
{
    s.depth.depthFmt = find_depth_format(s.pd);
    auto img = create_image(s.pd, s.device.get(),
        s.sc.extent.width, s.sc.extent.height,
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

static void create_renderpass(AppState& s)
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

void create_framebuffers(AppState& s)
{
    s.pipe.framebuffers.clear();
    s.pipe.framebuffers.reserve(s.sc.views.size());

    for (auto& v : s.sc.views)
    {
        std::array<vk::ImageView,2> views = { v.get(), s.depth.view.get() };
        s.pipe.framebuffers.push_back(s.device->createFramebufferUnique(vk::FramebufferCreateInfo{
            {}, s.pipe.renderPass.get(), (uint32_t)views.size(), views.data(),
            s.sc.extent.width, s.sc.extent.height, 1
        }));
    }
}

void create_cmd_buffers(AppState& s)
{
    s.cmdBuffers = s.device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
        s.cmdPool.get(), vk::CommandBufferLevel::ePrimary, (uint32_t)s.pipe.framebuffers.size()
    });
}

static void create_swapchain(AppState& s, IPlatformWindow& wnd)
{
    // Wait until we have a non-zero drawable size (minimized windows report 0x0).
    while (!VKMINI_HEADLESS && wnd.is_minimized())
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
        s.sc.surfFmt.format, s.sc.surfFmt.colorSpace,
        s.sc.extent,
        1, vk::ImageUsageFlagBits::eColorAttachment,
        concurrent ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
        concurrent ? (uint32_t)families.size() : 0u,
        concurrent ? families.data() : nullptr,
        caps.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        s.sc.presentMode,
        true,
        s.sc.swapchain ? s.sc.swapchain.get() : vk::SwapchainKHR{}
    );

    s.sc.swapchain = s.device->createSwapchainKHRUnique(sci);
    s.sc.images = s.device->getSwapchainImagesKHR(s.sc.swapchain.get());

    s.sc.views.clear();
    s.sc.views.reserve(s.sc.images.size());
    for (auto img : s.sc.images)
    {
        s.sc.views.push_back(s.device->createImageViewUnique(vk::ImageViewCreateInfo{
            {}, img, vk::ImageViewType::e2D, s.sc.surfFmt.format,
            {}, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0,1,0,1 }
        }));
    }

    // Recreate dependent resources
    create_depth(s);
    create_renderpass(s);
    setup_pipeline(s);
    create_framebuffers(s);
    create_cmd_buffers(s);

    s.sync.imagesInFlight.assign(s.sc.images.size(), vk::Fence{});
}

static void destroy_swapchain_deps(AppState& s)
{
    s.device->waitIdle();
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
}

void setup(AppState& s, IPlatformWindow& wnd)
{
    setup_instance(s);

    DebugMessenger dbg = create_debug_messenger(s.instance.get());
    // Store destroy function + handle via raw, no lifetime issue (instance outlives app).
    // We keep it local; destroying at end is handled in run.

    setup_surface(s, wnd);
    setup_device(s);
    setup_assets(s);
    create_swapchain(s, wnd);
    setup_sync(s);

    // keep messenger alive by stashing in static? simplest: leakless lambda in run handles destroy; run owns it.
    // We'll expose through thread_local globals:
    (void)dbg;
}

} // namespace vkmini
