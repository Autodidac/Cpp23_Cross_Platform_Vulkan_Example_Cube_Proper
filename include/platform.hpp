#pragma once
#include <cstdint>
#include <string_view>

namespace vkmini {

struct FramebufferSize { uint32_t w=0, h=0; };

struct NativeWindow {
#if defined(_WIN32)
    void* hinstance = nullptr;
    void* hwnd = nullptr;
#elif defined(__ANDROID__)
    void* app = nullptr;          // android_app*
    void* native_window = nullptr; // ANativeWindow*
#else
    void* xcb_connection = nullptr; // xcb_connection_t*
    std::uint32_t xcb_window = 0;   // xcb_window_t
#endif
};

class IPlatformWindow {
public:
    virtual ~IPlatformWindow() = default;

    virtual NativeWindow native() const = 0;
    virtual FramebufferSize framebuffer_size() const = 0;

    // Returns false when the app should quit.
    virtual bool pump_events() = 0;

    // Blocks until at least one event is available (used during resize/minimize).
    virtual void wait_events() = 0;

    // True if the window is currently minimized / has zero drawable size.
    virtual bool is_minimized() const = 0;

    virtual const char* platform_name() const = 0;
};

struct WindowCreateInfo {
    const char* title = "Vulkan App";
    uint32_t width = 1280;
    uint32_t height = 720;
};

IPlatformWindow* create_platform_window(const WindowCreateInfo& ci);
void destroy_platform_window(IPlatformWindow* wnd);

} // namespace vkmini
