// MIT. Shared geometry, alpha and numeric formatting keep every glass surface aligned.
#pragma once
#include "NativeDrawing.h"
#include <algorithm>
#include <cmath>
#include <string>
namespace HudStyle
{
inline constexpr int UiFontSize = 13;
inline constexpr UINT AppearanceChanged = WM_APP + 93;
inline constexpr int FlyoutWidth = 320, FieldTop = 48, RowHeight = 50, InputHeight = 34;
inline constexpr int InputLeft = 140, InputWidth = 164, Padding = 16;
inline constexpr float SliderLeft = 22, SliderRight = 298, SliderY = 28;
inline constexpr int MaxFontSize = 120, ToolbarCount = 6;
// Expand the former 0–5% tint range over 0–40% of the control.
inline double GlassOpacity(double percent)
{
    percent = std::clamp(percent, 0.0, 100.0);
    return (percent <= 40 ? percent / 8 : 5 + (percent - 40) * 95 / 60) / 100;
}
inline double GlassStrength(double percent)
{
    // Blend only the Composition host backdrop; keep glyphs and controls independent.
    const double t = std::clamp(percent / 40.0, 0.0, 1.0);
    const double blend = t * t * (3 - 2 * t);
    // A subtle lift: at most one percentage point more glass, with identical endpoints.
    return blend + 0.04 * blend * (1 - blend);
}
inline int OpacityControl(double opacity)
{
    opacity = std::clamp(opacity, 0.0, 1.0);
    return static_cast<int>(std::round(opacity <= 0.05 ? opacity * 800 : 40 + (opacity - 0.05) * 60 / 0.95));
}
inline int FlyoutHeight(int rows, bool error = false)
{
    return FieldTop + std::max(0, rows - 1) * RowHeight + InputHeight + Padding + (error ? 22 : 0);
}
inline Gdiplus::RectF ToolbarBox(int width, int item)
{
    return Gdiplus::RectF(static_cast<float>(width - 234 + 38 * item), 4, 32, 24);
}
inline int ToolbarAnchor(int width, int item)
{
    auto box = ToolbarBox(width, item);
    return static_cast<int>(box.X + box.Width / 2);
}
inline std::wstring OffsetText(int milliseconds)
{
    if (!milliseconds)
        return L"0";
    wchar_t text[32]{};
    swprintf_s(text, L"%+.3f", milliseconds / 1000.0);
    std::wstring result(text);
    while (result.back() == L'0')
        result.pop_back();
    if (result.back() == L'.')
        result.pop_back();
    return result;
}
inline Gdiplus::Color GlassTint(const std::wstring &hex, int opacity)
{
    wchar_t *end = nullptr;
    unsigned long rgb = hex.size() == 7 && hex[0] == L'#' ? wcstoul(hex.c_str() + 1, &end, 16) : 0;
    if (!end || *end)
        rgb = 0x121214;
    const auto alpha = static_cast<BYTE>(std::round(GlassOpacity(opacity) * 255));
    return Gdiplus::Color(alpha, static_cast<BYTE>(rgb >> 16), static_cast<BYTE>(rgb >> 8),
                          static_cast<BYTE>(rgb));
}
inline Gdiplus::Color InteractiveTint(Gdiplus::Color tint)
{
    // Layered-window hit testing skips alpha-zero pixels before WM_NCHITTEST.
    // One alpha step keeps blank areas interactive, including at the 0% setting.
    return Gdiplus::Color(std::max<BYTE>(1, tint.GetA()), tint.GetR(), tint.GetG(), tint.GetB());
}
} // namespace HudStyle
