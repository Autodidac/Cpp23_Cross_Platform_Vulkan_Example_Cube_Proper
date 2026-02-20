#include "platform.hpp"
#include <new>

namespace vkmini {

#if defined(_WIN32)
IPlatformWindow* create_win32_window(const WindowCreateInfo&);
void destroy_win32_window(IPlatformWindow*);
#elif defined(__ANDROID__)
IPlatformWindow* create_android_window(const WindowCreateInfo&);
void destroy_android_window(IPlatformWindow*);
#else
IPlatformWindow* create_xcb_window(const WindowCreateInfo&);
void destroy_xcb_window(IPlatformWindow*);
#endif

IPlatformWindow* create_platform_window(const WindowCreateInfo& ci)
{
#if defined(_WIN32)
    return create_win32_window(ci);
#elif defined(__ANDROID__)
    return create_android_window(ci);
#else
    return create_xcb_window(ci);
#endif
}

void destroy_platform_window(IPlatformWindow* wnd)
{
#if defined(_WIN32)
    destroy_win32_window(wnd);
#elif defined(__ANDROID__)
    destroy_android_window(wnd);
#else
    destroy_xcb_window(wnd);
#endif
}

} // namespace vkmini
