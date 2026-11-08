// MIT. Cached alpha-safe DirectWrite text on a software Direct2D/WIC surface.
#pragma once
#include "NativeDrawing.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
class NativeText
{
    template <class T> using Com = Microsoft::WRL::ComPtr<T>;
    Com<ID2D1Factory> factory;
    Com<IDWriteFactory> fonts;
    Com<IWICImagingFactory> imaging;
    Com<IWICBitmap> bitmap;
    Com<ID2D1RenderTarget> target;
    Com<ID2D1SolidColorBrush> brush;
    Com<ID2D1LinearGradientBrush> mask;
    Com<ID2D1Layer> layer;
    Com<ID2D1StrokeStyle> roundStroke;
    Com<IDWriteRenderingParams> rendering;
    std::vector<Com<IDWriteTextLayout>> layouts;
    std::vector<DWRITE_TEXT_METRICS> metrics;
    std::vector<float> fittedSizes;
    int width = 0, height = 0, smoothing = -1, measuredSize = 0, measuredWidth = 0, measuredHeight = 0;
    uint64_t layoutRevision = 0;
    bool drawing = false, pixelsValid = false;
    bool fading = true;
    HRESULT error = S_OK;

  public:
    HRESULT lastError() const
    {
        return error;
    }
    bool init()
    {
        if (factory && fonts && imaging && roundStroke)
            return true;
        factory.Reset();
        fonts.Reset();
        imaging.Reset();
        roundStroke.Reset();
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf())) ||
            FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                       reinterpret_cast<IUnknown **>(fonts.GetAddressOf()))) ||
            FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(imaging.GetAddressOf()))))
            return false;
        D2D1_STROKE_STYLE_PROPERTIES stroke = D2D1::StrokeStyleProperties();
        stroke.startCap = stroke.endCap = D2D1_CAP_STYLE_ROUND;
        return SUCCEEDED(factory->CreateStrokeStyle(stroke, nullptr, 0, roundStroke.GetAddressOf()));
    }
    void invalidate()
    {
        measuredSize = 0;
        pixelsValid = false;
    }
    void releaseSurface()
    {
        layer.Reset();
        mask.Reset();
        brush.Reset();
        target.Reset();
        bitmap.Reset();
        width = height = 0;
        drawing = pixelsValid = false;
    }
    void clearLayouts()
    {
        layouts.clear();
        metrics.clear();
        fittedSizes.clear();
        invalidate();
    }
    uint64_t surfaceBytes() const
    {
        return static_cast<uint64_t>(width) * height * 4;
    }
    bool prepare(const std::vector<std::wstring> &lines, int size, int wrapWidth = 0, int fitHeight = 0)
    {
        if (!init())
            return false;
        if (measuredSize == size && measuredWidth == wrapWidth && measuredHeight == fitHeight &&
            layouts.size() == lines.size())
            return true;
        std::vector<Com<IDWriteTextLayout>> nextLayouts;
        std::vector<DWRITE_TEXT_METRICS> nextMetrics;
        std::vector<float> nextSizes;
        nextLayouts.reserve(lines.size());
        nextMetrics.reserve(lines.size());
        nextSizes.reserve(lines.size());
        Com<IDWriteTextFormat> format;
        if (FAILED(fonts->CreateTextFormat(HudDrawing::FontName, nullptr, DWRITE_FONT_WEIGHT_BOLD,
                                           DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                           static_cast<float>(size), L"", format.GetAddressOf())))
            return false;
        format->SetWordWrapping(wrapWidth > 0 ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
        format->SetTextAlignment(wrapWidth > 0 ? DWRITE_TEXT_ALIGNMENT_CENTER
                                               : DWRITE_TEXT_ALIGNMENT_LEADING);
        for (const auto &line : lines)
        {
            Com<IDWriteTextLayout> layout;
            const float layoutHeight = std::max(2000.0f, size * static_cast<float>(line.size() + 1) * 2);
            if (FAILED(fonts->CreateTextLayout(line.c_str(), static_cast<UINT32>(line.size()), format.Get(),
                                               wrapWidth > 0 ? static_cast<float>(wrapWidth) : 100000,
                                               layoutHeight, layout.GetAddressOf())))
                return false;
            DWRITE_TEXT_METRICS m{};
            if (FAILED(layout->GetMetrics(&m)))
                return false;
            float fitted = static_cast<float>(size);
            // Reflow before shrinking: long phrases may use any number of rows.
            // Only phrases taller than the viewport need a smaller font.
            if (wrapWidth > 0 && fitHeight > 0 && m.height > fitHeight)
            {
                float low = 1, high = fitted;
                const DWRITE_TEXT_RANGE range{0, static_cast<UINT32>(line.size())};
                while (high - low > 0.125f)
                {
                    const float candidate = (low + high) / 2;
                    if (FAILED(layout->SetFontSize(candidate, range)) || FAILED(layout->GetMetrics(&m)))
                        return false;
                    if (m.height <= fitHeight)
                        low = candidate;
                    else
                        high = candidate;
                }
                fitted = low;
                if (FAILED(layout->SetFontSize(fitted, range)) || FAILED(layout->GetMetrics(&m)))
                    return false;
            }
            nextLayouts.push_back(std::move(layout));
            nextMetrics.push_back(m);
            nextSizes.push_back(fitted);
        }
        layouts = std::move(nextLayouts);
        metrics = std::move(nextMetrics);
        fittedSizes = std::move(nextSizes);
        measuredSize = size;
        measuredWidth = wrapWidth;
        measuredHeight = fitHeight;
        ++layoutRevision;
        pixelsValid = false;
        return true;
    }
    uint64_t revision() const
    {
        return layoutRevision;
    }
    float textHeight(size_t i) const
    {
        return i < metrics.size() ? metrics[i].height : 0;
    }
    float layoutWidth(size_t i) const
    {
        return i < metrics.size() ? metrics[i].layoutWidth : 0;
    }
    UINT32 rowCount(size_t i) const
    {
        return i < metrics.size() ? metrics[i].lineCount : 0;
    }
    float fittedSize(size_t i) const
    {
        return i < fittedSizes.size() ? fittedSizes[i] : 0;
    }
    float textWidth(size_t i) const
    {
        return i < metrics.size() ? metrics[i].widthIncludingTrailingWhitespace : 0;
    }
    bool begin(int w, int h, float top, int amount, bool fadeEdges = true)
    {
        if (!init())
            return false;
        if (!target || width != w || height != h)
        {
            target.Reset();
            bitmap.Reset();
            brush.Reset();
            mask.Reset();
            layer.Reset();
            pixelsValid = false;
            if (FAILED(imaging->CreateBitmap(w, h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad,
                                             bitmap.GetAddressOf())))
                return false;
            auto properties = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
            if (FAILED(
                    factory->CreateWicBitmapRenderTarget(bitmap.Get(), properties, target.GetAddressOf())) ||
                FAILED(target->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), brush.GetAddressOf())))
            {
                target.Reset();
                return false;
            }
            width = w;
            height = h;
        }
        if (fadeEdges && !mask)
        {
            const D2D1_GRADIENT_STOP stops[] = {{0, D2D1::ColorF(1, 1, 1, 0)},
                                                {0.18f, D2D1::ColorF(1, 1, 1, 1)},
                                                {0.82f, D2D1::ColorF(1, 1, 1, 1)},
                                                {1, D2D1::ColorF(1, 1, 1, 0)}};
            Com<ID2D1GradientStopCollection> gradient;
            if (FAILED(target->CreateLayer(layer.GetAddressOf())) ||
                FAILED(target->CreateGradientStopCollection(stops, 4, gradient.GetAddressOf())) ||
                FAILED(target->CreateLinearGradientBrush(
                    D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, top),
                                                        D2D1::Point2F(0, static_cast<float>(h))),
                    gradient.Get(), mask.GetAddressOf())))
            {
                target.Reset();
                return false;
            }
        }
        amount = std::clamp(amount, 0, 100);
        if (smoothing != amount || !rendering)
        {
            const float t = amount / 100.0f;
            rendering.Reset();
            if (FAILED(fonts->CreateCustomRenderingParams(
                    1.4f + 0.8f * t, 1.5f * (1 - t), 0, DWRITE_PIXEL_GEOMETRY_FLAT,
                    amount ? DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC : DWRITE_RENDERING_MODE_ALIASED,
                    rendering.GetAddressOf())))
                return false;
            smoothing = amount;
        }
        target->BeginDraw();
        pixelsValid = false;
        drawing = true;
        target->SetTransform(D2D1::Matrix3x2F::Identity());
        target->Clear(D2D1::ColorF(0, 0, 0, 0));
        target->SetTextAntialiasMode(amount ? D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE
                                            : D2D1_TEXT_ANTIALIAS_MODE_ALIASED);
        target->SetTextRenderingParams(rendering.Get());
        fading = fadeEdges;
        if (fading)
        {
            mask->SetStartPoint(D2D1::Point2F(0, top));
            mask->SetEndPoint(D2D1::Point2F(0, static_cast<float>(h)));
            target->PushLayer(
                D2D1::LayerParameters(D2D1::RectF(18, top, static_cast<float>(w - 18), static_cast<float>(h)),
                                      nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                      D2D1::Matrix3x2F::Identity(), 1, mask.Get()),
                layer.Get());
        }
        return true;
    }
    void draw(size_t i, float x, float centerY, float scale, Gdiplus::Color color, float maxWidth = 0,
              bool snap = false)
    {
        if (!drawing || i >= layouts.size())
            return;
        // Stable lines snap to physical pixels; transitions retain fractional movement.
        float y = centerY - metrics[i].height * scale / 2;
        if (snap)
        {
            x = std::round(x);
            y = std::round(y);
        }
        if (maxWidth > 0)
        {
            target->SetTransform(D2D1::Matrix3x2F::Identity());
            target->PushAxisAlignedClip(D2D1::RectF(x, y, x + maxWidth, y + metrics[i].height * scale),
                                        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        }
        target->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(x, y));
        brush->SetColor(D2D1::ColorF(color.GetR() / 255.0f, color.GetG() / 255.0f, color.GetB() / 255.0f,
                                     color.GetA() / 255.0f));
        target->DrawTextLayout(D2D1::Point2F(0, 0), layouts[i].Get(), brush.Get(),
                               snap ? D2D1_DRAW_TEXT_OPTIONS_NONE : D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        if (maxWidth > 0)
            target->PopAxisAlignedClip();
    }
    void underline(float x, float y, float barWidth, double breath)
    {
        if (!drawing)
            return;
        target->SetTransform(D2D1::Matrix3x2F::Identity());
        auto a = D2D1::Point2F(x - barWidth / 2 + 1.5f, y), b = D2D1::Point2F(x + barWidth / 2 - 1.5f, y);
        for (int glow = 9; glow >= 0; --glow)
        {
            float opacity = static_cast<float>((0.55 + 0.45 * breath) * (glow ? (10 - glow) * 2 / 255.0 : 1));
            brush->SetColor(D2D1::ColorF(53 / 255.0f, 208 / 255.0f, 127 / 255.0f, opacity));
            target->DrawLine(a, b, brush.Get(), 3.0f + glow, roundStroke.Get());
        }
    }
    bool composite(Gdiplus::Graphics &g)
    {
        if (!drawing)
            return false;
        target->SetTransform(D2D1::Matrix3x2F::Identity());
        if (fading)
            target->PopLayer();
        drawing = false;
        error = target->EndDraw();
        if (FAILED(error))
        {
            target.Reset();
            return false;
        }
        pixelsValid = true;
        return compositeCached(g);
    }
    bool compositeCached(Gdiplus::Graphics &g)
    {
        if (drawing || !pixelsValid || !bitmap)
            return false;
        Com<IWICBitmapLock> lock;
        WICRect rect{0, 0, width, height};
        if (FAILED(bitmap->Lock(&rect, WICBitmapLockRead, lock.GetAddressOf())))
            return false;
        UINT stride = 0, size = 0;
        BYTE *pixels = nullptr;
        if (FAILED(lock->GetStride(&stride)) || FAILED(lock->GetDataPointer(&size, &pixels)))
            return false;
        Gdiplus::Bitmap surface(width, height, static_cast<INT>(stride), PixelFormat32bppPARGB, pixels);
        return g.DrawImage(&surface, 0, 0) == Gdiplus::Ok;
    }
};
