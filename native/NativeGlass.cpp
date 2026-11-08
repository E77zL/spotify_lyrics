// MIT. Windows Composition blends host blur on the GPU; no screen capture or polling renderer.
#include "NativeGlass.h"
#include "NativeStyle.h"
#include <dwmapi.h>
#include <roapi.h>
#include <DispatcherQueue.h>
#include <windows.ui.composition.interop.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.Foundation.Numerics.h>
using namespace winrt;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Composition::Desktop;
namespace
{
struct CompositionThread
{
    bool initialized = false;
    HRESULT failure = S_OK;
    winrt::Windows::System::DispatcherQueueController queue{nullptr};
    Compositor compositor{nullptr};
    CompositionThread()
    {
        try
        {
            check_hresult(RoInitialize(RO_INIT_SINGLETHREADED));
            initialized = true;
            if (!winrt::Windows::System::DispatcherQueue::GetForCurrentThread())
            {
                DispatcherQueueOptions options{sizeof(options), DQTYPE_THREAD_CURRENT, DQTAT_COM_STA};
                check_hresult(CreateDispatcherQueueController(
                    options,
                    reinterpret_cast<ABI::Windows::System::IDispatcherQueueController **>(put_abi(queue))));
            }
            compositor = Compositor();
        }
        catch (...)
        {
            failure = to_hresult();
            compositor = nullptr;
        }
    }
    ~CompositionThread()
    {
        compositor = nullptr;
        queue = nullptr;
        if (initialized)
            RoUninitialize();
    }
};
LRESULT CALLBACK GlassProc(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_NCHITTEST)
        return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE)
        return MA_NOACTIVATE;
    if (message == WM_ERASEBKGND)
        return 1;
    return DefWindowProcW(window, message, w, l);
}
} // namespace
void ApplyHudFrame(HWND window)
{
    // The foreground alpha and Composition geometry own the same rounded edge.
    // An integer region or a DWM border would cut it again and expose seams.
    SetWindowRgn(window, nullptr, FALSE);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    const COLORREF border = DWMWA_COLOR_NONE;
    DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &border, sizeof(border));
}
struct NativeGlass::State
{
    HWND foreground{}, backdrop{}, previousOwner{};
    DesktopWindowTarget target{nullptr};
    ContainerVisual root{nullptr};
    SpriteVisual blur{nullptr};
    CompositionRoundedRectangleGeometry geometry{nullptr};
    RECT bounds{};
    int radius = -1;
    float amount = -1;
    bool shown = false, syncing = false;
    ~State()
    {
        if (IsWindow(foreground) && GetWindow(foreground, GW_OWNER) == backdrop)
            SetWindowLongPtrW(foreground, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(previousOwner));
        try
        {
            if (target)
            {
                target.Root(nullptr);
                target.Close();
            }
        }
        catch (...)
        {
        }
        if (IsWindow(backdrop))
            DestroyWindow(backdrop);
    }
};
NativeGlass::NativeGlass() = default;
NativeGlass::~NativeGlass() = default;
void NativeGlass::release()
{
    state.reset();
    attempted = false;
    failure = S_OK;
}
HWND NativeGlass::window() const
{
    return state ? state->backdrop : nullptr;
}
double NativeGlass::strength() const
{
    return state ? std::max(0.0f, state->amount) : 0;
}
void NativeGlass::attach(HWND foreground)
{
    if (attempted || !IsWindow(foreground))
        return;
    attempted = true;
    try
    {
        static thread_local CompositionThread runtime;
        if (!runtime.compositor)
        {
            failure = runtime.failure;
            return;
        }
        auto next = std::make_unique<State>();
        next->foreground = foreground;
        next->previousOwner = GetWindow(foreground, GW_OWNER);
        auto module = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(foreground, GWLP_HINSTANCE));
        WNDCLASSW cls{};
        cls.lpfnWndProc = GlassProc;
        cls.hInstance = module;
        cls.lpszClassName = L"SpotifyNativeCompositionGlass";
        RegisterClassW(&cls);
        next->backdrop = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                                             WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                         cls.lpszClassName, L"", WS_POPUP | WS_DISABLED, 0, 0, 1, 1,
                                         next->previousOwner, nullptr, module, nullptr);
        if (!next->backdrop)
        {
            failure = HRESULT_FROM_WIN32(GetLastError());
            return;
        }
        ApplyHudFrame(next->backdrop);
        // WS_EX_TRANSPARENT skips input across processes only on layered windows.
        // Keep window alpha fixed; the Composition visual controls the actual blur blend.
        check_bool(SetLayeredWindowAttributes(next->backdrop, 0, 255, LWA_ALPHA));
        BOOL enabled = TRUE;
        // Windows 11 host backdrop; unsupported systems retain the shared translucent tint.
        check_hresult(
            DwmSetWindowAttribute(next->backdrop, DWMWA_USE_HOSTBACKDROPBRUSH, &enabled, sizeof(enabled)));
        const auto interop =
            runtime.compositor.as<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop>();
        check_hresult(interop->CreateDesktopWindowTarget(
            next->backdrop, true,
            reinterpret_cast<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget **>(
                put_abi(next->target))));
        next->root = runtime.compositor.CreateContainerVisual();
        next->geometry = runtime.compositor.CreateRoundedRectangleGeometry();
        next->root.Clip(runtime.compositor.CreateGeometricClip(next->geometry));
        next->blur = runtime.compositor.CreateSpriteVisual();
        next->blur.RelativeSizeAdjustment({1, 1});
        next->blur.Brush(runtime.compositor.CreateHostBackdropBrush());
        next->blur.Opacity(0);
        next->root.Children().InsertAtTop(next->blur);
        next->target.Root(next->root);
        // The foreground is owned by its backdrop, so its text always stays above the blur.
        SetWindowLongPtrW(foreground, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(next->backdrop));
        state = std::move(next);
    }
    catch (...)
    {
        failure = to_hresult();
        state.reset();
    }
}
void NativeGlass::sync(int radius, int opacity, double fade)
{
    if (!state || state->syncing || !IsWindow(state->foreground))
        return;
    auto &s = *state;
    s.syncing = true;
    try
    {
        RECT next{};
        GetWindowRect(s.foreground, &next);
        const bool resized = next.right - next.left != s.bounds.right - s.bounds.left ||
                             next.bottom - next.top != s.bounds.bottom - s.bounds.top || s.radius != radius;
        if (resized)
        {
            const winrt::Windows::Foundation::Numerics::float2 size{
                static_cast<float>(next.right - next.left), static_cast<float>(next.bottom - next.top)};
            s.root.Size(size);
            s.geometry.Size(size);
            s.geometry.CornerRadius({static_cast<float>(radius), static_cast<float>(radius)});
            s.radius = radius;
        }
        if (!EqualRect(&s.bounds, &next))
        {
            s.bounds = next;
            SetWindowPos(s.backdrop, nullptr, next.left, next.top, next.right - next.left,
                         next.bottom - next.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
        // At 100% the solid foreground covers the background; skip blur entirely.
        const float amount =
            opacity >= 100
                ? 0
                : static_cast<float>(HudStyle::GlassStrength(opacity) * std::clamp(fade, 0.0, 1.0));
        if (s.amount != amount)
        {
            s.amount = amount;
            s.blur.Opacity(amount);
        }
        const bool visible = amount > 0 && IsWindowVisible(s.foreground);
        if (s.shown != visible)
        {
            s.shown = visible;
            ShowWindow(s.backdrop, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
            if (visible)
                SetWindowPos(s.foreground, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
    }
    catch (...)
    {
        failure = to_hresult();
        ShowWindow(s.backdrop, SW_HIDE);
        s.shown = false;
    }
    s.syncing = false;
}
