#if !defined(_WIN32) && !defined(__ANDROID__)

#include "platform.hpp"
#include <xcb/xcb.h>
#include <cstdlib>
#include <new>

namespace vkmini {
namespace {

class XcbWindow final : public IPlatformWindow {
public:
    explicit XcbWindow(const WindowCreateInfo& ci)
    {
        conn_ = xcb_connect(nullptr, nullptr);
        if (int err = xcb_connection_has_error(conn_); err) { ok_ = false; return; }

        const xcb_setup_t* setup = xcb_get_setup(conn_);
        xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
        screen_ = it.data;

        window_ = xcb_generate_id(conn_);
        uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        uint32_t values[2] = {
            screen_->black_pixel,
            XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY |
            XCB_EVENT_MASK_KEY_PRESS
        };

        xcb_create_window(conn_,
                          XCB_COPY_FROM_PARENT,
                          window_,
                          screen_->root,
                          0, 0,
                          (uint16_t)ci.width, (uint16_t)ci.height,
                          0,
                          XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          screen_->root_visual,
                          mask,
                          values);

        xcb_change_property(conn_, XCB_PROP_MODE_REPLACE, window_,
                            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                            std::strlen(ci.title), ci.title);

        xcb_map_window(conn_, window_);
        xcb_flush(conn_);

        fbw_ = ci.width; fbh_ = ci.height;
    }

    ~XcbWindow() override
    {
        if (conn_)
        {
            if (window_) xcb_destroy_window(conn_, window_);
            xcb_disconnect(conn_);
        }
    }

    NativeWindow native() const override { return NativeWindow{ (void*)conn_, window_ }; }
    FramebufferSize framebuffer_size() const override { return FramebufferSize{ fbw_, fbh_ }; }

    bool pump_events() override
    {
        for(;;)
        {
            xcb_generic_event_t* ev = xcb_poll_for_event(conn_);
            if (!ev) break;
            handle(ev);
            std::free(ev);
        }
        return alive_ && ok_;
    }

    void wait_events() override
    {
        xcb_generic_event_t* ev = xcb_wait_for_event(conn_);
        if (!ev) return;
        handle(ev);
        std::free(ev);
    }

    bool is_minimized() const override { return minimized_ || fbw_ == 0 || fbh_ == 0; }
    const char* platform_name() const override { return "xcb"; }

private:
    void handle(xcb_generic_event_t* ev)
    {
        const uint8_t type = ev->response_type & ~0x80;
        switch(type)
        {
        case XCB_DESTROY_NOTIFY: alive_ = false; break;
        case XCB_CONFIGURE_NOTIFY:
        {
            auto* c = (xcb_configure_notify_event_t*)ev;
            fbw_ = c->width;
            fbh_ = c->height;
            minimized_ = (fbw_ == 0 || fbh_ == 0);
        } break;
        case XCB_KEY_PRESS:
            alive_ = false;
            break;
        default: break;
        }
    }

    xcb_connection_t* conn_ = nullptr;
    xcb_screen_t* screen_ = nullptr;
    xcb_window_t window_ = 0;
    uint32_t fbw_ = 0, fbh_ = 0;
    bool minimized_ = false;
    bool alive_ = true;
    bool ok_ = true;
};

} // namespace

IPlatformWindow* create_xcb_window(const WindowCreateInfo& ci) { return new (std::nothrow) XcbWindow(ci); }
void destroy_xcb_window(IPlatformWindow* wnd) { delete wnd; }

} // namespace vkmini

#endif
