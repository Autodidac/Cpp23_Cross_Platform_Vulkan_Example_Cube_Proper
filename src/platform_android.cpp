#if defined(__ANDROID__)

#include "platform.hpp"
#include <new>

namespace vkmini {
namespace {
class AndroidWindow final : public IPlatformWindow {
public:
    explicit AndroidWindow(const WindowCreateInfo&) {}
    NativeWindow native() const override { return {}; }
    FramebufferSize framebuffer_size() const override { return {}; }
    bool pump_events() override { return false; }
    void wait_events() override {}
    bool is_minimized() const override { return true; }
    const char* platform_name() const override { return "android(stub)"; }
};
}
IPlatformWindow* create_android_window(const WindowCreateInfo& ci) { return new (std::nothrow) AndroidWindow(ci); }
void destroy_android_window(IPlatformWindow* wnd) { delete wnd; }
} // namespace vkmini

#endif
