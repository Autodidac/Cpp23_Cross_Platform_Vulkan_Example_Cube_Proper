#include "vk_app.hpp"
#include "vk_state.hpp"
#include "vk_internal.hpp"

namespace vkmini {

void VkApp::run(IPlatformWindow& window)
{
    AppState s{};
    setup(s, window);
    run_loop(s, window);
}

} // namespace vkmini
