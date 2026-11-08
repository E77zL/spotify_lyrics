// MIT. Shared typography and vector icons for the HUD and settings.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <vector>
namespace HudDrawing
{
// Cache just the partial corner pixels. Mask the premultiplied surface once;
// Windows and the blur compositor must not add a second, different outline.
class RoundedEdges
{
    struct Pixel
    {
        int x, y, alpha;
    };
    int cachedRadius = -1;
    std::vector<Pixel> edge;

  public:
    void apply(void *pixels, int width, int height, int radius)
    {
        radius = std::clamp(radius, 0, std::min(width, height) / 2);
        if (cachedRadius != radius)
        {
            cachedRadius = radius;
            edge.clear();
            constexpr int samples = 8;
            for (int y = 0; y < radius; ++y)
                for (int x = 0; x < radius; ++x)
                {
                    int inside = 0;
                    for (int sy = 0; sy < samples; ++sy)
                        for (int sx = 0; sx < samples; ++sx)
                        {
                            const int dx = (radius - x) * samples * 2 - sx * 2 - 1;
                            const int dy = (radius - y) * samples * 2 - sy * 2 - 1;
                            const int r = radius * samples * 2;
                            inside += dx * dx + dy * dy <= r * r;
                        }
                    if (inside < samples * samples)
                        edge.push_back({x, y, (inside * 255 + samples * samples / 2) / (samples * samples)});
                }
        }
        auto bytes = static_cast<BYTE *>(pixels);
        for (const auto &p : edge)
            for (int corner = 0; corner < 4; ++corner)
            {
                const int x = corner & 1 ? width - p.x - 1 : p.x;
                const int y = corner & 2 ? height - p.y - 1 : p.y;
                auto pixel = bytes + (y * width + x) * 4;
                for (int channel = 0; channel < 4; ++channel)
                    pixel[channel] = static_cast<BYTE>((pixel[channel] * p.alpha + 127) / 255);
            }
    }
};
inline constexpr wchar_t FontName[] = L"Segoe UI";
enum class Icon
{
    Close,
    Fullscreen,
    Restore,
    Opacity,
    Hash,
    Minus,
    Plus
};
inline void Rounded(Gdiplus::Graphics &g, Gdiplus::RectF box, float radius, Gdiplus::Color fill,
                    Gdiplus::Color edge = Gdiplus::Color(0, 0, 0, 0))
{
    Gdiplus::GraphicsPath p;
    const float d = radius * 2;
    p.AddArc(box.X, box.Y, d, d, 180, 90);
    p.AddArc(box.GetRight() - d, box.Y, d, d, 270, 90);
    p.AddArc(box.GetRight() - d, box.GetBottom() - d, d, d, 0, 90);
    p.AddArc(box.X, box.GetBottom() - d, d, d, 90, 90);
    p.CloseFigure();
    if (fill.GetA())
    {
        Gdiplus::SolidBrush brush(fill);
        g.FillPath(&brush, &p);
    }
    if (edge.GetA())
    {
        Gdiplus::Pen pen(edge);
        g.DrawPath(&pen, &p);
    }
}
inline void DrawIcon(Gdiplus::Graphics &g, Icon icon, Gdiplus::RectF box, Gdiplus::Color color)
{
    auto state = g.Save();
    g.TranslateTransform(box.X, box.Y);
    g.ScaleTransform(box.Width / 24, box.Height / 24);
    Gdiplus::Pen pen(color, 1.8f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    if (icon == Icon::Close)
    {
        g.DrawLine(&pen, 6, 6, 18, 18);
        g.DrawLine(&pen, 18, 6, 6, 18);
    }
    else if (icon == Icon::Fullscreen || icon == Icon::Restore)
    {
        for (int i = 0; i < 4; ++i)
        {
            auto corner = g.Save();
            g.TranslateTransform(12, 12);
            g.RotateTransform(static_cast<float>(i * 90));
            const float p = icon == Icon::Fullscreen ? -8.0f : -3.0f;
            const float q = icon == Icon::Fullscreen ? -3.0f : -8.0f;
            g.DrawLine(&pen, p, q, p, p);
            g.DrawLine(&pen, p, p, q, p);
            g.Restore(corner);
        }
    }
    else if (icon == Icon::Opacity)
    {
        g.DrawEllipse(&pen, 4.0f, 4.0f, 16.0f, 16.0f);
        Gdiplus::SolidBrush brush(color);
        g.FillPie(&brush, 4, 4, 16, 16, -90, 180);
    }
    else if (icon == Icon::Hash)
    {
        g.DrawLine(&pen, 9, 4, 7, 20);
        g.DrawLine(&pen, 17, 4, 15, 20);
        g.DrawLine(&pen, 4, 9, 20, 9);
        g.DrawLine(&pen, 3, 15, 19, 15);
    }
    else
    {
        g.DrawLine(&pen, 6, 12, 18, 12);
        if (icon == Icon::Plus)
            g.DrawLine(&pen, 12, 6, 12, 18);
    }
    g.Restore(state);
}
} // namespace HudDrawing
