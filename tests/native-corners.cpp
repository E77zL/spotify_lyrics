// MIT. Inspect the actual HUD and flyout pixels submitted to Windows.
#include "../native/NativeCore.h"
#include "../native/NativeFlyout.h"
#include <array>
#include <cstdio>

static std::vector<BYTE> submitted;
static SIZE submittedSize{};
static BOOL WINAPI CaptureSurface(HWND window, HDC screen, POINT *at, SIZE *size,
                                  HDC source, POINT *origin, COLORREF key,
                                  BLENDFUNCTION *blend, DWORD flags)
{
    DIBSECTION dib{};
    if (source && GetObjectW(GetCurrentObject(source, OBJ_BITMAP), sizeof(dib), &dib) && dib.dsBm.bmBits)
    {
        submittedSize = *size;
        const auto bits = static_cast<const BYTE *>(dib.dsBm.bmBits);
        submitted.assign(bits, bits + size->cx * size->cy * 4);
    }
    return UpdateLayeredWindow(window, screen, at, size, source, origin, key, blend, flags);
}
// Intercept the submission in the test translation unit, without production hooks.
#define UpdateLayeredWindow CaptureSurface
#include "../native/NativeHud.cpp"
#include "../native/NativeFlyout.cpp"
#undef UpdateLayeredWindow

static bool Check(HWND window, const char *label, int radius, int opacity)
{
    const int width = submittedSize.cx, height = submittedSize.cy;
    if (!window || submitted.empty() || width <= 0 || height <= 0)
        return false;
    HRGN shape = CreateRectRgn(0, 0, 0, 0);
    bool passed = GetWindowRgn(window, shape) == ERROR;
    DeleteObject(shape);
    const int expected = HudStyle::InteractiveTint(HudStyle::GlassTint(L"#448888", opacity)).GetA();
    std::array<int, 4> missing{}, leaking{}, smooth{};
    for (int dy = 0; dy < radius + 2; ++dy)
        for (int dx = 0; dx < radius + 2; ++dx)
        {
            const double distance = std::hypot(std::max(0.0, radius - dx - 0.5),
                                               std::max(0.0, radius - dy - 0.5));
            int firstAlpha = -1;
            for (int corner = 0; corner < 4; ++corner)
            {
                const int x = corner & 1 ? width - dx - 1 : dx;
                const int y = corner & 2 ? height - dy - 1 : dy;
                const auto pixel = &submitted[(y * width + x) * 4];
                const int alpha = pixel[3];
                if (distance < radius - 1 && alpha < expected)
                    ++missing[corner];
                if (distance > radius + 1 && alpha)
                    ++leaking[corner];
                if (alpha > 0 && alpha < 255)
                    ++smooth[corner];
                for (int channel = 0; channel < 3; ++channel)
                    passed &= pixel[channel] <= alpha;
                if (opacity == 100)
                {
                    if (firstAlpha < 0)
                        firstAlpha = alpha;
                    passed &= alpha == firstAlpha;
                }
            }
        }
    for (int corner = 0; corner < 4; ++corner)
        passed &= !missing[corner] && !leaking[corner] && (opacity != 100 || smooth[corner] > 10);
    if (!passed)
        printf("FAIL %s %dx%d r=%d opacity=%d missing=[%d,%d,%d,%d] leaking=[%d,%d,%d,%d]\n",
               label, width, height, radius, opacity, missing[0], missing[1], missing[2], missing[3],
               leaking[0], leaking[1], leaking[2], leaking[3]);
    return passed;
}

int wmain()
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        return 1;
    GdiplusStartupInput input;
    ULONG_PTR token{};
    if (GdiplusStartup(&token, &input, nullptr) != Ok)
        return 1;
    bool passed = true;
    for (const auto size : {SIZE{420, 110}, SIZE{903, 237}, SIZE{2000, 1000}})
        for (const int opacity : {0, 1, 5, 40, 59, 99, 100})
        {
            Hud h;
            h.width = size.cx;
            h.height = size.cy;
            h.alpha = 1;
            h.status.clear();
            h.settings = {{"bgColor", "#448888"}, {"transparency", opacity / 100.0}};
            h.refreshStyle();
            h.window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                      L"STATIC", L"Corner coverage", WS_POPUP, 100, 100,
                                      h.width, h.height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (!h.window)
                return 1;
            h.paint(1.0 / 60);
            passed &= h.painted && Check(h.window, "HUD", 16, opacity);
            h.glass.release();
            h.releaseCanvas();
            DestroyWindow(h.window);
        }
    {
        NativeFlyout form;
        for (const int opacity : {0, 1, 5, 40, 59, 99, 100})
            for (const auto kind : {FlyoutKind::Timing, FlyoutKind::Typography, FlyoutKind::Opacity,
                                   FlyoutKind::Colors})
            {
                HudAppearance appearance;
                appearance.background = L"#448888";
                appearance.opacity = opacity;
                form.show(GetModuleHandleW(nullptr), nullptr, {100, 100, 870, 323}, 700, kind, appearance,
                          [](const HudAppearance &, std::wstring &) { return true; });
                const int radius = static_cast<int>(std::round(14 * GetDpiForWindow(form.window) / 96.0));
                passed &= Check(form.window, "flyout", radius, opacity);
                form.hide(false);
            }
    }
    GdiplusShutdown(token);
    CoUninitialize();
    if (passed)
        puts("PASS actual HUD and all four flyout surfaces: tint coverage, smooth symmetric corners, "
             "premultiplied edges and transparent/opaque endpoints");
    return passed ? 0 : 1;
}
