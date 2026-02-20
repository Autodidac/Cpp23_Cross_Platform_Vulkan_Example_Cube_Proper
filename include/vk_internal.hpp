#pragma once
#include "vk_state.hpp"
#include "platform.hpp"

namespace vkmini {

// setup entrypoint used by VkApp
void setup(AppState& s, IPlatformWindow& wnd);

// reusable pieces for swapchain recreation
void setup_pipeline(AppState& s);
void create_framebuffers(AppState& s);
void create_cmd_buffers(AppState& s);

// main loop
void run_loop(AppState& s, IPlatformWindow& wnd);

} // namespace vkmini
