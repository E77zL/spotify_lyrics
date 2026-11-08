// MIT. Exercise actual alpha surfaces, smoothing, resize and settings validation.
#include "../native/NativeText.h"
#include "../native/NativeFlyout.h"
#include <dwmapi.h>
#include <cassert>
#include <cstdio>
using namespace Gdiplus;
static void CheckRoundedFrame(HWND window)
{
    assert(window);
    DWM_WINDOW_CORNER_PREFERENCE corner{};
    // Older Windows versions do not support these attributes.
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner))))
        assert(corner == DWMWCP_DONOTROUND);
    COLORREF border{};
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_BORDER_COLOR, &border, sizeof(border))))
        assert(border == DWMWA_COLOR_NONE);
}
static std::vector<BYTE> Render(NativeText &text, int smoothing, int width = 420, float x = 24,
                                bool reuse = false)
{
    Bitmap bitmap(width, 160, PixelFormat32bppPARGB);
    {
        Graphics g(&bitmap);
        g.Clear(Color(0, 0, 0, 0));
        if (!reuse)
        {
            assert(text.begin(width, 160, 0, smoothing));
            text.draw(0, x, 80, 1, Color(255, 245, 245, 240));
        }
        if (!(reuse ? text.compositeCached(g) : text.composite(g)))
        {
            printf("Render failed: %08lx, smoothing=%d\n", text.lastError(), smoothing);
            fflush(stdout);
            std::abort();
        }
    }
    BitmapData bits{};
    Rect rect(0, 0, width, 160);
    assert(bitmap.LockBits(&rect, ImageLockModeRead, PixelFormat32bppPARGB, &bits) == Ok);
    std::vector<BYTE> pixels(static_cast<BYTE *>(bits.Scan0),
                             static_cast<BYTE *>(bits.Scan0) + width * 160 * 4);
    bitmap.UnlockBits(&bits);
    return pixels;
}
static size_t Edges(const std::vector<BYTE> &pixels)
{
    size_t count = 0;
    for (size_t i = 3; i < pixels.size(); i += 4)
        if (pixels[i] > 0 && pixels[i] < 255)
            ++count;
    return count;
}
static std::vector<BYTE> Scene(NativeText &text, float center, float scale, bool cropped, bool fade)
{
    constexpr int w = 1200, h = 300;
    Bitmap bitmap(w, h, PixelFormat32bppPARGB);
    {
        Graphics g(&bitmap);
        g.Clear(Color(0, 0, 0, 0));
        assert(text.begin(w, h, 32, 50, fade, cropped ? 500 : 0));
        text.draw(0, (w - text.layoutWidth(0) * scale) / 2, center, scale, Color(245, 245, 245, 240));
        text.underline(w / 2, center + text.textHeight(0) * scale / 2 + 4, 36, 0.5);
        assert(text.composite(g));
    }
    BitmapData bits{};
    Rect rect(0, 0, w, h);
    assert(bitmap.LockBits(&rect, ImageLockModeRead, PixelFormat32bppPARGB, &bits) == Ok);
    std::vector<BYTE> pixels(static_cast<BYTE *>(bits.Scan0), static_cast<BYTE *>(bits.Scan0) + w * h * 4);
    bitmap.UnlockBits(&bits);
    return pixels;
}
int wmain()
{
    assert(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
    GdiplusStartupInput input;
    ULONG_PTR token = 0;
    assert(GdiplusStartup(&token, &input, nullptr) == Ok);
    {
        NativeText renderer;
        assert(renderer.prepare({L"Small bold lyrics — 夜空"}, 14));
        auto aliased = Render(renderer, 0), balanced = Render(renderer, 50), smooth = Render(renderer, 100);
        assert(Edges(balanced) > 50);
        assert(Edges(aliased) < Edges(balanced));
        assert(balanced != smooth);
        assert(Render(renderer, 50) == balanced);
        assert(Render(renderer, 50, 420, 24, true) == balanced);
        const auto cachedRevision = renderer.revision();
        assert(renderer.surfaceBytes() == 420 * 160 * 4);
        renderer.releaseSurface();
        assert(renderer.surfaceBytes() == 0);
        assert(renderer.prepare({L"Small bold lyrics — 夜空"}, 14));
        assert(renderer.revision() == cachedRevision);
        assert(Render(renderer, 50) == balanced);
        assert(Render(renderer, 50, 420, 24.1f) != Render(renderer, 50, 420, 24.4f));
        assert(Render(renderer, 50, 800).size() == 800 * 160 * 4);
        // A reused, cropped raster must match a newly cleared full-size raster,
        // including scrolling, zooming, glow and both viewport fade boundaries.
        for (bool fade : {false, true})
        {
            NativeText cropped;
            assert(cropped.prepare({L"Smooth moving lyrics"}, 26, 1152));
            for (float center : {150.0f, 151.25f, 60.5f, -50.0f, 330.0f, 140.0f})
                for (float scale : {1.0f, 0.68f, 0.873f})
                {
                    NativeText fresh;
                    assert(fresh.prepare({L"Smooth moving lyrics"}, 26, 1152));
                    assert(Scene(cropped, center, scale, true, fade) == Scene(fresh, center, scale, false, fade));
                    assert(cropped.surfaceBytes() == 500 * 300 * 4);
                }
        }
        renderer.invalidate();
        assert(renderer.prepare({L"i"}, 14));
        assert(renderer.textWidth(0) < 15);
        NativeText wrapped;
        std::wstring paragraph;
        for (int i = 0; i < 25; ++i)
            paragraph += L"Long lyrics keep every word and wrap into as many rows as needed. ";
        assert(wrapped.prepare({paragraph}, 26, 372, 0));
        assert(wrapped.rowCount(0) > 12 && wrapped.textWidth(0) <= 372.5f);
        const auto narrowRows = wrapped.rowCount(0);
        const auto revision = wrapped.revision();
        assert(wrapped.prepare({paragraph}, 26, 372, 0) && wrapped.revision() == revision);
        assert(wrapped.prepare({paragraph}, 26, 952, 0));
        assert(wrapped.rowCount(0) < narrowRows);
        assert(wrapped.prepare({paragraph}, 120, 372, 340));
        assert(wrapped.rowCount(0) > 5 && wrapped.textHeight(0) <= 340);
        assert(wrapped.fittedSize(0) < 120);
        assert(wrapped.prepare({paragraph}, 120, 372, 37));
        assert(wrapped.textHeight(0) <= 37 && wrapped.textWidth(0) <= 372.5f);
        // Exercise real glyph pixels, with no clipping mask to hide overflow.
        Bitmap narrow(420, 110, PixelFormat32bppPARGB);
        {
            Graphics g(&narrow);
            g.Clear(Color(0, 0, 0, 0));
            assert(wrapped.begin(420, 110, 0, 50, false));
            wrapped.draw(0, 24, 55, 1, Color(255, 245, 245, 240));
            assert(wrapped.composite(g));
        }
        int ink = 0;
        for (int y = 0; y < 110; ++y)
            for (int x = 0; x < 420; ++x)
            {
                Color pixel;
                narrow.GetPixel(x, y, &pixel);
                if (pixel.GetA())
                {
                    ++ink;
                    assert(x >= 22 && x <= 398 && y >= 35 && y <= 75);
                }
            }
        assert(ink > 100);
        wrapped.invalidate();
        assert(wrapped.prepare({std::wstring(250, L'W'), std::wstring(250, L'夜'), L"First\nSecond\nThird"},
                               26, 372));
        assert(wrapped.rowCount(0) > 5 && wrapped.textWidth(0) <= 372.5f);
        assert(wrapped.rowCount(1) > 5 && wrapped.textWidth(1) <= 372.5f);
        assert(wrapped.rowCount(2) == 3);
        NativeFlyout form;
        HudAppearance initial, applied = initial;
        int saves = 0;
        auto onApply = [&](const HudAppearance &v, std::wstring &) {
            applied = v;
            ++saves;
            return true;
        };
        RECT lyrics{100, 100, 870, 323};
        form.show(GetModuleHandleW(nullptr), nullptr, lyrics, 700, FlyoutKind::Typography, initial, onApply);
        assert(form.window);
        const auto style = GetWindowLongPtrW(form.window, GWL_STYLE);
        assert(!(style & WS_CAPTION));
        assert(!(style & WS_SYSMENU));
        assert(!GetWindow(form.window, GW_CHILD)); // No Windows buttons, edits or their white frames.
        const float dpi = GetDpiForWindow(form.window) / 96.0f;
        auto click = [&](int x, int y) {
            SendMessageW(form.window, WM_LBUTTONDOWN, MK_LBUTTON,
                         MAKELPARAM(static_cast<int>(x * dpi), static_cast<int>(y * dpi)));
        };
        auto enter = [&](int row, const wchar_t *value) {
            click(222, (form.kind == FlyoutKind::Typography || form.kind == FlyoutKind::Opacity ? 53 : 65) +
                           row * 50);
            for (const wchar_t *p = value; *p; ++p)
                SendMessageW(form.window, WM_CHAR, *p, 0);
            SendMessageW(form.window, WM_KEYDOWN, VK_RETURN, 0);
        };
        enter(1, L"101");
        assert(saves == 0);
        enter(1, L"75");
        assert(saves == 1 && applied.antialias == 75);
        enter(0, L"24");
        assert(saves == 2 && applied.fontSize == 24);
        RECT popup{};
        GetWindowRect(form.window, &popup);
        assert(popup.top >= lyrics.bottom);
        assert(popup.right - popup.left == static_cast<int>(320 * dpi));
        assert(popup.bottom - popup.top == static_cast<int>(148 * dpi));
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromRect(&lyrics, MONITOR_DEFAULTTONEAREST), &monitor);
        lyrics.top = monitor.rcWork.bottom - 100;
        lyrics.bottom = monitor.rcWork.bottom - 5;
        form.reposition(lyrics, 700);
        GetWindowRect(form.window, &popup);
        assert(popup.bottom <= lyrics.top);
        form.hide(false);
        form.show(GetModuleHandleW(nullptr), nullptr, lyrics, 700, FlyoutKind::Colors, applied, onApply);
        enter(0, L"#fff");
        assert(saves == 2);
        enter(0, L"#141416");
        assert(saves == 3 && applied.background == L"#141416");
        enter(0, L"abcdef");
        assert(saves == 4 && applied.background == L"#abcdef");
        SendMessageW(form.window, WM_KEYDOWN, VK_ESCAPE, 0);
        assert(!form.window && saves == 4);
        form.show(GetModuleHandleW(nullptr), nullptr, lyrics, 700, FlyoutKind::Timing, applied, onApply);
        GetWindowRect(form.window, &popup);
        assert(popup.bottom - popup.top == static_cast<int>(98 * dpi));
        enter(0, L"-1.5");
        assert(applied.offsetMs == -1500 && saves == 5);
        click(288, 65);
        assert(applied.offsetMs == -1000 && saves == 6);
        enter(0, L"+1");
        assert(applied.offsetMs == 1000 && saves == 7);
        enter(0, L"0");
        assert(applied.offsetMs == 0 && saves == 8);
        enter(0, L"61");
        assert(applied.offsetMs == 0 && saves == 8);
        form.hide(false);
        form.show(GetModuleHandleW(nullptr), nullptr, lyrics, 700, FlyoutKind::Opacity, applied, onApply);
        enter(0, L"25");
        assert(applied.opacity == 25 && form.opacity() == 25 && saves == 9);
        enter(0, L"0");
        assert(applied.opacity == 0 && form.opacity() == 0 && saves == 10);
        form.hide(false);
        int previews = 0;
        const int baseline = saves;
        form.show(GetModuleHandleW(nullptr), nullptr, lyrics, 700, FlyoutKind::Opacity, applied, onApply,
                  [&](const HudAppearance &) { ++previews; });
        click(160, 76);
        SendMessageW(form.window, WM_MOUSEMOVE, MK_LBUTTON,
                     MAKELPARAM(static_cast<int>(298 * dpi), static_cast<int>(76 * dpi)));
        assert(saves == baseline && previews >= 1);
        SendMessageW(form.window, WM_LBUTTONUP, 0, 0);
        assert(saves == baseline + 1 && applied.opacity == 100);
        SendMessageW(form.window, WM_KEYDOWN, VK_TAB, 0);
        SendMessageW(form.window, WM_CHAR, L'3', 0);
        SendMessageW(form.window, WM_CHAR, L'7', 0);
        SendMessageW(form.window, WM_KEYDOWN, VK_RETURN, 0);
        assert(saves == baseline + 2 && applied.opacity == 37);
        click(22, 76);
        form.hide(false);
        assert(saves == baseline + 2 && applied.opacity == 37);
        assert(HudStyle::GlassStrength(0) == 0);
        assert(HudStyle::GlassStrength(1) < 0.002);
        assert(std::abs(HudStyle::GlassStrength(20) - 0.51) < 0.000001);
        assert(HudStyle::GlassStrength(40) == 1);
        for (int value = 1; value <= 100; ++value)
        {
            assert(HudStyle::GlassStrength(value) >= HudStyle::GlassStrength(value - 1));
            assert(HudStyle::GlassStrength(value) - HudStyle::GlassStrength(value - 1) < 0.038);
            if (value >= 40)
                assert(HudStyle::GlassStrength(value) == 1);
        }
        assert(static_cast<BYTE>(std::round(255 * HudStyle::GlassStrength(0))) == 0);
        assert(static_cast<BYTE>(std::round(255 * HudStyle::GlassStrength(1))) == 0);
        assert(static_cast<BYTE>(std::round(255 * HudStyle::GlassStrength(4))) > 0);
        assert(HudStyle::GlassTint(L"#121214", 0).GetA() == 0);
        assert(HudStyle::InteractiveTint(HudStyle::GlassTint(L"#121214", 0)).GetA() == 1);
        assert(HudStyle::InteractiveTint(HudStyle::GlassTint(L"#121214", 100)).GetA() == 255);
        assert(HudStyle::GlassTint(L"#121214", 40).GetA() == 13);
        assert(HudStyle::GlassTint(L"#121214", 100).GetA() == 255);
        assert(HudStyle::OpacityControl(0.05) == 40);
        assert(HudStyle::OpacityControl(0.0) == 0);
        assert(HudStyle::OpacityControl(1.0) == 100);
        // Reproduce the opaque teal corner regression for every actual flyout window,
        // and repeat with its translucent Composition backdrop enabled.
        form.hide(false);
        for (int opacity : {100, 40})
            for (auto kind : {FlyoutKind::Timing, FlyoutKind::Typography, FlyoutKind::Opacity,
                              FlyoutKind::Colors})
            {
                auto appearance = applied;
                appearance.background = L"#448888";
                appearance.opacity = opacity;
                form.show(GetModuleHandleW(nullptr), nullptr, lyrics, 700, kind, appearance, onApply);
                CheckRoundedFrame(form.window);
                if (const auto backdrop = GetWindow(form.window, GW_OWNER))
                    CheckRoundedFrame(backdrop);
                form.hide(false);
            }
        // Exercise a real host-backdrop visual without changing foreground opacity or input.
        HWND host = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC",
                                    L"Composition QA", WS_POPUP, 100, 100, 300, 140, nullptr, nullptr,
                                    GetModuleHandleW(nullptr), nullptr);
        assert(host);
        ApplyHudFrame(host);
        CheckRoundedFrame(host);
        SetLayeredWindowAttributes(host, 0, 255, LWA_ALPHA);
        ShowWindow(host, SW_SHOWNOACTIVATE);
        NativeGlass composition;
        composition.attach(host);
        const HWND backdrop = composition.window();
        printf("Composition backdrop: %s (%08lx)\n", backdrop ? "active" : "unavailable",
               composition.error());
        if (backdrop)
        {
            CheckRoundedFrame(backdrop);
            assert(GetWindow(host, GW_OWNER) == backdrop);
            const auto ex = GetWindowLongPtrW(backdrop, GWL_EXSTYLE);
            assert(!IsWindowEnabled(backdrop));
            assert((ex & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT)) ==
                   (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT));
            for (int value : {0, 1, 5, 10, 20, 30, 39, 40, 60, 99, 100})
            {
                composition.sync(16, value);
                const double expected = value == 100 ? 0 : HudStyle::GlassStrength(value);
                assert(std::abs(composition.strength() - expected) < 0.000001);
                assert(!!IsWindowVisible(backdrop) == (expected > 0));
                assert(IsWindowVisible(host));
                assert(GetForegroundWindow() != backdrop);
            }
            composition.sync(16, 20, 0.5);
            assert(std::abs(composition.strength() - 0.255) < 0.000001);
            // Game click-through must not stop on the new compositor surface.
            SetWindowLongPtrW(host, GWL_EXSTYLE, GetWindowLongPtrW(host, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
            assert(WindowFromPoint({150, 150}) != backdrop);
            SetWindowPos(host, nullptr, 170, 180, 450, 220, SWP_NOZORDER | SWP_NOACTIVATE);
            composition.sync(16, 20);
            RECT frontBounds{}, backBounds{};
            GetWindowRect(host, &frontBounds);
            GetWindowRect(backdrop, &backBounds);
            assert(EqualRect(&frontBounds, &backBounds));
            ShowWindow(host, SW_HIDE);
            composition.sync(16, 20);
            assert(!IsWindowVisible(backdrop));
            composition.release();
            assert(!IsWindow(backdrop) && IsWindow(host) && !GetWindow(host, GW_OWNER));
        }
        DestroyWindow(host);
        form.show(GetModuleHandleW(nullptr), nullptr, lyrics, 700, FlyoutKind::Typography, applied, onApply);
        enter(0, L"96");
        assert(applied.fontSize == 96);
        enter(0, L"121");
        assert(applied.fontSize == 96);
        for (int i = 0; i < HudStyle::ToolbarCount; ++i)
        {
            auto box = HudStyle::ToolbarBox(770, i);
            assert(box.Width == 32 && box.Height == 24 && box.Y == 4);
            if (i)
                assert(box.X - HudStyle::ToolbarBox(770, i - 1).GetRight() == 6);
        }
    }
    GdiplusShutdown(token);
    CoUninitialize();
    puts("PASS uncapped multiline wrapping, height fitting, Unicode/long words, cached reflow, alpha edges, "
         "smoothing, resize, inline glass flyouts, "
         "input validation, stepping, above/below placement and Escape");
}
