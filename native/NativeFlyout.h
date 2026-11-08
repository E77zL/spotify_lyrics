// MIT. Compact inline appearance controls; no system edit/button widgets.
#pragma once
#include "NativeText.h"
#include "NativeStyle.h"
#include "NativeGlass.h"
#include <functional>
struct HudAppearance
{
    int fontSize = 22, opacity = 55, antialias = 50, offsetMs = 0;
    std::wstring background = L"#121214", foreground = L"#f5f5f0";
};
enum class FlyoutKind
{
    Timing,
    Typography,
    Opacity,
    Colors
};
class NativeFlyout
{
    struct Field
    {
        const wchar_t *label;
        int *number;
        int min, max, step;
        const wchar_t *unit;
        std::wstring *color;
        bool seconds = false;
    };
    HMODULE module{};
    HWND owner{};
    HudAppearance current;
    std::vector<Field> fields;
    std::function<bool(const HudAppearance &, std::wstring &)> apply;
    std::function<void(const HudAppearance &)> preview;
    HudAppearance dragStart;
    NativeText text;
    NativeGlass glass;
    HudDrawing::RoundedEdges corners;
    std::vector<std::wstring> textValues;
    std::wstring edit, notice;
    int active = -1, rasterWidth = 0, rasterHeight = 0;
    int focusedRow = 0, dragging = -1;
    HDC canvas{};
    HBITMAP raster{}, previousRaster{};
    void *pixels{};
    int canvasWidth = 0, canvasHeight = 0;
    RECT anchorRect{};
    int anchorCenterX = 0;
    bool selectAll = false, caret = false;
    float dpi = 1;
    ULONGLONG editDeadline = 0, inactiveSince = 0, blink = 0;
    int px(double n) const;
    bool commit(bool report);
    void beginEdit(int row);
    void slide(float x);
    void finishSlide(bool save);
    bool sliders() const
    {
        return kind == FlyoutKind::Typography || kind == FlyoutKind::Opacity;
    }
    std::wstring fieldText(int row) const;
    void paint();
    void releaseCanvas();
    LRESULT dispatch(UINT message, WPARAM w, LPARAM l);
    static LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM w, LPARAM l);

  public:
    HWND window{};
    FlyoutKind kind = FlyoutKind::Typography;
    int opacity() const
    {
        return current.opacity;
    }
    ~NativeFlyout();
    void show(HMODULE instance, HWND parent, RECT lyrics, int anchorX, FlyoutKind category,
              const HudAppearance &value, std::function<bool(const HudAppearance &, std::wstring &)> onApply,
              std::function<void(const HudAppearance &)> onPreview = {});
    void reposition(RECT lyrics, int anchorX);
    void hide(bool applyPending = true);
};
