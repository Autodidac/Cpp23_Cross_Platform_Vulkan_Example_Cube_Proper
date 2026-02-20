#include "vk_app.hpp"
#include "platform.hpp"
#include <iostream>

int main()
{
#if VKMINI_HEADLESS
    std::cout << "[vkmini] Headless build: no window/swapchain.\n";
    return 0;
#else
    using namespace vkmini;
    WindowCreateInfo ci{};
    ci.title = "vk_cross_platform_default";
    if (auto* wnd = create_platform_window(ci))
    {
        VkApp app{};
        try { app.run(*wnd); }
        catch (const std::exception& e)
        {
            std::cerr << "Fatal: " << e.what() << "\n";
            destroy_platform_window(wnd);
            return 1;
        }
        destroy_platform_window(wnd);
        return 0;
    }
    std::cerr << "Failed to create platform window.\n";
    return 1;
#endif
}
