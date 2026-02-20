#if defined(_WIN32)

#include "platform.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace vkmini {

namespace {
class Win32Window final : public IPlatformWindow {
public:
    explicit Win32Window(const WindowCreateInfo& ci)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Win32Window::WndProcThunk;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"vkmini_win32";
        RegisterClassExW(&wc);

        std::wstring title;
        for (const char* p = ci.title; *p; ++p) title.push_back((wchar_t)(unsigned char)(*p));

        RECT r{0,0,(LONG)ci.width,(LONG)ci.height};
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

        hwnd_ = CreateWindowExW(
            0, wc.lpszClassName, title.c_str(),
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, r.right-r.left, r.bottom-r.top,
            nullptr, nullptr, wc.hInstance, this);

        hinst_ = wc.hInstance;
	    // Initialize framebuffer size immediately; WM_SIZE may not have fired yet.
	    RECT rc{};
	    if (hwnd_ && GetClientRect(hwnd_, &rc))
	    {
	        fbw_ = (uint32_t)((rc.right > rc.left) ? (rc.right - rc.left) : 0);
	        fbh_ = (uint32_t)((rc.bottom > rc.top) ? (rc.bottom - rc.top) : 0);
	    }
        width_ = ci.width;
        height_ = ci.height;
    }

    ~Win32Window() override
    {
        if (hwnd_) DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    NativeWindow native() const override { return NativeWindow{ (void*)hinst_, (void*)hwnd_ }; }

    FramebufferSize framebuffer_size() const override { return FramebufferSize{ fbw_, fbh_ }; }

    bool pump_events() override
    {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) return false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return alive_;
    }

    void wait_events() override
    {
        MSG msg{};
        GetMessageW(&msg, nullptr, 0, 0);
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    bool is_minimized() const override { return minimized_ || fbw_ == 0 || fbh_ == 0; }

    const char* platform_name() const override { return "win32"; }

private:
    static LRESULT CALLBACK WndProcThunk(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        auto* self = (Win32Window*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (m == WM_NCCREATE)
        {
            auto* cs = (CREATESTRUCTW*)l;
            self = (Win32Window*)cs->lpCreateParams;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
        }
        return self ? self->WndProc(h,m,w,l) : DefWindowProcW(h,m,w,l);
    }

    LRESULT WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        switch(m)
        {
        case WM_CLOSE: alive_ = false; PostQuitMessage(0); return 0;
        case WM_SIZE:
            minimized_ = (w == SIZE_MINIMIZED);
            fbw_ = (uint32_t)LOWORD(l);
            fbh_ = (uint32_t)HIWORD(l);
            return 0;
        default: break;
        }
        return DefWindowProcW(h,m,w,l);
    }

    HINSTANCE hinst_{};
    HWND hwnd_{};
    uint32_t width_{}, height_{};
    uint32_t fbw_{0}, fbh_{0};
    bool minimized_ = false;
    bool alive_ = true;
};
}

IPlatformWindow* create_win32_window(const WindowCreateInfo& ci) { return new (std::nothrow) Win32Window(ci); }
void destroy_win32_window(IPlatformWindow* wnd) { delete wnd; }

} // namespace vkmini
#endif
