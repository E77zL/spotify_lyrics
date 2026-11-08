#include "NativeFlyout.h"
#include "NativeGlass.h"
#include <windowsx.h>
using namespace Gdiplus;
namespace
{
using namespace HudStyle;
const wchar_t *Title(FlyoutKind kind)
{
    switch (kind)
    {
    case FlyoutKind::Typography:
        return L"Typography";
    case FlyoutKind::Timing:
        return L"Lyric timing";
    case FlyoutKind::Opacity:
        return L"Opacity";
    default:
        return L"Colors";
    }
}
} // namespace
int NativeFlyout::px(double n) const
{
    return static_cast<int>(std::round(n * dpi));
}
NativeFlyout::~NativeFlyout()
{
    hide(false);
    releaseCanvas();
}
void NativeFlyout::releaseCanvas()
{
    if (!canvas)
        return;
    SelectObject(canvas, previousRaster);
    DeleteObject(raster);
    DeleteDC(canvas);
    canvas = nullptr;
    raster = nullptr;
    pixels = nullptr;
}
void NativeFlyout::show(HMODULE instance, HWND parent, RECT lyrics, int anchorX, FlyoutKind category,
                        const HudAppearance &value,
                        std::function<bool(const HudAppearance &, std::wstring &)> onApply,
                        std::function<void(const HudAppearance &)> onPreview)
{
    hide();
    module = instance;
    owner = parent;
    kind = category;
    current = value;
    apply = std::move(onApply);
    preview = std::move(onPreview);
    focusedRow = 0;
    dragging = -1;
    active = -1;
    edit.clear();
    notice.clear();
    editDeadline = inactiveSince = 0;
    fields.clear();
    switch (kind)
    {
    case FlyoutKind::Typography:
        fields = {{L"Font size", &current.fontSize, 14, HudStyle::MaxFontSize, 2, L" px", nullptr},
                  {L"Anti-aliasing", &current.antialias, 0, 100, 5, L"%", nullptr}};
        break;
    case FlyoutKind::Timing:
        fields = {{L"Offset", &current.offsetMs, -60000, 60000, 500, L" s", nullptr, true}};
        break;
    case FlyoutKind::Opacity:
        fields = {{L"Opacity", &current.opacity, 0, 100, 5, L"%", nullptr}};
        break;
    case FlyoutKind::Colors:
        fields = {{L"Background", nullptr, 0, 0, 0, L"", &current.background},
                  {L"Lyrics", nullptr, 0, 0, 0, L"", &current.foreground}};
        break;
    }
    WNDCLASSW cls{};
    cls.lpfnWndProc = Proc;
    cls.hInstance = module;
    cls.lpszClassName = L"SpotifyNativeLyricsFlyout";
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&cls);
    window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST, cls.lpszClassName, Title(kind),
                             WS_POPUP, lyrics.left, lyrics.bottom, FlyoutWidth, 200, parent, nullptr, module,
                             this);
    if (!window)
        return;
    ApplyHudFrame(window);
    dpi = GetDpiForWindow(window) / 96.0f;
    glass.attach(window);
    reposition(lyrics, anchorX);
    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
    SetFocus(window);
    paint();
    SetTimer(window, 1, 100, nullptr);
}
void NativeFlyout::reposition(RECT lyrics, int anchorX)
{
    if (!window)
        return;
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(MonitorFromRect(&lyrics, MONITOR_DEFAULTTONEAREST), &info);
    rasterWidth = px(FlyoutWidth);
    anchorRect = lyrics;
    anchorCenterX = anchorX;
    rasterHeight = px(FlyoutHeight(static_cast<int>(fields.size()), !notice.empty()));
    int x = std::clamp(
        anchorX - rasterWidth / 2, static_cast<int>(info.rcWork.left),
        std::max(static_cast<int>(info.rcWork.left), static_cast<int>(info.rcWork.right) - rasterWidth));
    int y = lyrics.bottom + px(8);
    if (y + rasterHeight > info.rcWork.bottom)
        y = lyrics.top - rasterHeight - px(8);
    y = std::clamp(
        y, static_cast<int>(info.rcWork.top),
        std::max(static_cast<int>(info.rcWork.top), static_cast<int>(info.rcWork.bottom) - rasterHeight));
    SetWindowPos(window, HWND_TOPMOST, x, y, rasterWidth, rasterHeight, SWP_NOACTIVATE);
    glass.sync(px(14), current.opacity);
}
bool NativeFlyout::commit(bool report)
{
    editDeadline = 0;
    if (active < 0)
        return true;
    auto &field = fields[active];
    int converted = 0;
    bool valid = false;
    if (field.number)
    {
        if (field.seconds)
        {
            wchar_t *end = nullptr;
            const double value = wcstod(edit.c_str(), &end) * 1000;
            valid = !edit.empty() && end == edit.c_str() + edit.size() && std::isfinite(value) &&
                    value >= field.min && value <= field.max;
            if (valid)
                converted = static_cast<int>(std::round(value));
        }
        else
        {
            valid = !edit.empty() && edit.find_first_not_of(L"0123456789") == std::wstring::npos;
            if (valid)
                converted = _wtoi(edit.c_str());
            valid = valid && converted >= field.min && converted <= field.max;
        }
    }
    else
    {
        auto hex = edit;
        if (hex.size() == 6)
            hex.insert(hex.begin(), L'#');
        valid = hex.size() == 7 && hex[0] == L'#' &&
                hex.find_first_not_of(L"0123456789abcdefABCDEF", 1) == std::wstring::npos;
        if (valid)
            edit = std::move(hex);
    }
    if (!valid)
    {
        if (report)
            notice = field.number
                         ? (field.seconds
                                ? L"−60 to +60 s"
                                : std::to_wstring(field.min) + L"–" + std::to_wstring(field.max) + field.unit)
                         : L"Use RRGGBB or #RRGGBB";
        paint();
        return false;
    }
    if (field.number ? *field.number == converted : *field.color == edit)
    {
        notice.clear();
        paint();
        return true;
    }
    auto previous = current;
    if (field.number)
        *field.number = converted;
    else
        *field.color = edit;
    std::wstring error;
    if (!apply(current, error))
    {
        current = previous;
        notice = error;
        paint();
        return false;
    }
    notice.clear();
    if (previous.opacity != current.opacity)
        glass.sync(px(14), current.opacity);
    paint();
    return true;
}
std::wstring NativeFlyout::fieldText(int row) const
{
    const auto &field = fields[row];
    return field.number
               ? (field.seconds ? HudStyle::OffsetText(*field.number) : std::to_wstring(*field.number))
               : *field.color;
}
void NativeFlyout::beginEdit(int row)
{
    if (!commit(true))
        return;
    active = focusedRow = row;
    edit = fieldText(row);
    selectAll = caret = true;
    blink = GetTickCount64();
    paint();
}
void NativeFlyout::slide(float x)
{
    if (dragging < 0)
        return;
    auto &field = fields[dragging];
    const float fraction = std::clamp((x - SliderLeft) / (SliderRight - SliderLeft), 0.0f, 1.0f);
    const int next = field.min + static_cast<int>(std::round(fraction * (field.max - field.min)));
    if (next == *field.number)
        return;
    *field.number = next;
    if (preview)
        preview(current);
    if (kind == FlyoutKind::Opacity)
        glass.sync(px(14), current.opacity);
    paint();
}
void NativeFlyout::finishSlide(bool save)
{
    if (dragging < 0)
        return;
    dragging = -1;
    if (GetCapture() == window)
        ReleaseCapture();
    std::wstring error;
    if (!save || !apply(current, error))
    {
        current = dragStart;
        if (preview)
            preview(current);
        notice = std::move(error);
    }
    if (kind == FlyoutKind::Opacity)
        glass.sync(px(14), current.opacity);
    paint();
}
void NativeFlyout::hide(bool applyPending)
{
    if (window)
    {
        finishSlide(applyPending);
        if (applyPending)
            commit(false);
        DestroyWindow(window);
    }
}
void NativeFlyout::paint()
{
    if (!window || rasterWidth <= 0 || rasterHeight <= 0)
        return;
    if (rasterHeight != px(FlyoutHeight(static_cast<int>(fields.size()), !notice.empty())))
        reposition(anchorRect, anchorCenterX);
    HDC screen = GetDC(nullptr);
    if (!canvas || canvasWidth != rasterWidth || canvasHeight != rasterHeight)
    {
        releaseCanvas();
        canvas = CreateCompatibleDC(screen);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = rasterWidth;
        info.bmiHeader.biHeight = -rasterHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        raster = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!raster)
        {
            DeleteDC(canvas);
            canvas = nullptr;
            ReleaseDC(nullptr, screen);
            return;
        }
        previousRaster = static_cast<HBITMAP>(SelectObject(canvas, raster));
        canvasWidth = rasterWidth;
        canvasHeight = rasterHeight;
    }
    Bitmap surface(rasterWidth, rasterHeight, rasterWidth * 4, PixelFormat32bppPARGB,
                   static_cast<BYTE *>(pixels));
    Graphics g(&surface);
    // Match the HUD: fill once, then apply the cached smooth edge.
    g.Clear(InteractiveTint(GlassTint(current.background, current.opacity)));
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    HudDrawing::Rounded(g, RectF(0.5f, 0.5f, rasterWidth - 1.0f, rasterHeight - 1.0f), px(14) - 0.5f,
                        Color(0, 0, 0, 0),
                        Color(static_cast<BYTE>(std::min(44, current.opacity)), 255, 255, 255));
    g.ScaleTransform(dpi, dpi);
    struct Label
    {
        std::wstring value;
        float x, y, scale, maxWidth;
        Color color;
        bool center;
    };
    std::vector<Label> labels{{Title(kind), Padding, 24, 14 / 13.0f, 0, Color(245, 245, 245, 240), false}};
    for (int i = 0; i < static_cast<int>(fields.size()); ++i)
    {
        auto &field = fields[i];
        const float y = static_cast<float>(FieldTop + i * RowHeight);
        const bool editing = active == i;
        if (sliders() && !editing)
        {
            labels.push_back({field.label, Padding, y + 5, 1, 180, Color(220, 245, 245, 240), false});
            auto value = fieldText(i) + field.unit;
            labels.push_back(
                {value, FlyoutWidth - Padding - 28.0f, y + 5, 1, 64, Color(255, 245, 245, 240), true});
            const float cy = y + SliderY;
            const float fraction = static_cast<float>(*field.number - field.min) / (field.max - field.min);
            const float knob = SliderLeft + fraction * (SliderRight - SliderLeft);
            HudDrawing::Rounded(g, RectF(SliderLeft, cy - 2, SliderRight - SliderLeft, 4), 2,
                                Color(48, 245, 245, 240));
            if (knob > SliderLeft)
                HudDrawing::Rounded(g, RectF(SliderLeft, cy - 2, std::max(4.0f, knob - SliderLeft), 4), 2,
                                    Color(165, 53, 208, 127));
            SolidBrush halo(Color(dragging == i ? 45 : 20, 53, 208, 127));
            g.FillEllipse(&halo, knob - 10, cy - 10, 20.0f, 20.0f);
            SolidBrush thumb(Color(255, 245, 245, 240));
            g.FillEllipse(&thumb, knob - 5, cy - 5, 10.0f, 10.0f);
            continue;
        }
        HudDrawing::Rounded(g, RectF(InputLeft, y, InputWidth, InputHeight), 8, Color(36, 255, 255, 255),
                            editing ? Color(200, 53, 208, 127) : Color(35, 255, 255, 255));
        labels.push_back({field.label, Padding, y + InputHeight / 2.0f, 1, InputLeft - Padding - 12.0f,
                          Color(220, 245, 245, 240), false});
        std::wstring value = editing ? edit : fieldText(i);
        if (field.number && !editing)
            value += field.unit;
        if (editing && selectAll)
            HudDrawing::Rounded(
                g, RectF(field.number ? 180.0f : 150.0f, y + 6, field.number ? 84.0f : 144.0f, 22), 4,
                Color(65, 53, 208, 127));
        labels.push_back({value, field.number ? 222.0f : 152.0f, y + InputHeight / 2.0f, 1,
                          field.number ? 84.0f : 140.0f, Color(255, 245, 245, 240), field.number != nullptr});
        if (field.number && !sliders())
        {
            HudDrawing::DrawIcon(g, HudDrawing::Icon::Minus, RectF(146, y + 7, 20, 20),
                                 Color(220, 245, 245, 240));
            HudDrawing::DrawIcon(g, HudDrawing::Icon::Plus, RectF(278, y + 7, 20, 20),
                                 Color(220, 245, 245, 240));
        }
    }
    if (!notice.empty())
        labels.push_back({notice, Padding, rasterHeight / dpi - 16, 11 / 13.0f, FlyoutWidth - Padding * 2.0f,
                          Color(255, 241, 139, 139), false});
    g.ResetTransform();
    std::vector<std::wstring> values;
    for (const auto &label : labels)
        values.push_back(label.value);
    if (values != textValues)
    {
        textValues = std::move(values);
        text.invalidate();
    }
    if (text.prepare(textValues, px(UiFontSize)) && text.begin(rasterWidth, rasterHeight, 0, 50, false))
    {
        for (size_t i = 0; i < labels.size(); ++i)
        {
            auto &label = labels[i];
            float x = label.x * dpi;
            if (label.center)
                x -= text.textWidth(i) * label.scale / 2;
            text.draw(i, x, label.y * dpi, label.scale, label.color, label.maxWidth * dpi, true);
        }
        text.composite(g);
        if (active >= 0 && !selectAll && caret)
        {
            const float length = text.textWidth(2 + active * 2), center = 222 * dpi;
            float x = fields[active].number ? center + length / 2 + 2 * dpi : 152 * dpi + length + 2 * dpi;
            const float y = (FieldTop + active * RowHeight + 9) * dpi;
            Pen pen(Color(230, 245, 245, 240));
            g.DrawLine(&pen, x, y, x, y + 16 * dpi);
        }
    }
    g.Flush(FlushIntentionSync);
    corners.apply(pixels, rasterWidth, rasterHeight, px(14));
    RECT bounds{};
    GetWindowRect(window, &bounds);
    POINT at{bounds.left, bounds.top}, origin{};
    SIZE size{rasterWidth, rasterHeight};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(window, screen, &at, &size, canvas, &origin, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
    glass.sync(px(14), current.opacity);
}
LRESULT CALLBACK NativeFlyout::Proc(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
    auto self = reinterpret_cast<NativeFlyout *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        self = static_cast<NativeFlyout *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        self->window = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    try
    {
        if (self)
            return self->dispatch(message, w, l);
    }
    catch (...)
    { /* Host process stays isolated from UI errors. */
    }
    return DefWindowProcW(hwnd, message, w, l);
}
LRESULT NativeFlyout::dispatch(UINT message, WPARAM w, LPARAM l)
{
    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(window, &ps);
        EndPaint(window, &ps);
        paint();
        return 0;
    }
    case WM_CLOSE:
        hide();
        return 0;
    case WM_NCDESTROY: {
        HWND old = window;
        window = nullptr;
        glass.release();
        fields.clear();
        active = -1;
        releaseCanvas();
        text.releaseSurface();
        if (IsWindow(owner))
            PostMessageW(owner, HudStyle::AppearanceChanged, 0, 0);
        SetWindowLongPtrW(old, GWLP_USERDATA, 0);
        return DefWindowProcW(old, message, w, l);
    }
    case WM_ACTIVATE:
        inactiveSince = LOWORD(w) == WA_INACTIVE ? GetTickCount64() : 0;
        return 0;
    case WM_TIMER:
        if (dragging < 0 && inactiveSince && GetTickCount64() - inactiveSince > 180)
        {
            hide();
            return 0;
        }
        if (editDeadline && GetTickCount64() >= editDeadline)
            commit(false);
        if (active >= 0 && !selectAll && GetForegroundWindow() == window && GetTickCount64() - blink >= 500)
        {
            blink = GetTickCount64();
            caret = !caret;
            paint();
        }
        return 0;
    case WM_LBUTTONDOWN: {
        const float x = GET_X_LPARAM(l) / dpi, y = GET_Y_LPARAM(l) / dpi;
        const int row = static_cast<int>((y - FieldTop) / RowHeight);
        if (sliders() && y >= FieldTop - 8 && row >= 0 && row < static_cast<int>(fields.size()) &&
            x >= Padding && x <= FlyoutWidth - Padding && y <= FieldTop + row * RowHeight + InputHeight)
        {
            if (active == row)
            {
                selectAll = true;
                SetFocus(window);
                paint();
                return 0;
            }
            if (!commit(true))
                return 0;
            active = -1;
            focusedRow = row;
            if (y < FieldTop + row * RowHeight + 14)
                beginEdit(row);
            else
            {
                dragging = row;
                dragStart = current;
                SetCapture(window);
                slide(x);
            }
            SetFocus(window);
            return 0;
        }
        if (y < FieldTop || row < 0 || row >= static_cast<int>(fields.size()) || x < InputLeft ||
            x > InputLeft + InputWidth || y - (FieldTop + row * RowHeight) > InputHeight)
        {
            commit(true);
            active = -1;
            paint();
            return 0;
        }
        if (active != row)
        {
            commit(false);
            active = row;
            edit = fieldText(row);
        }
        auto &field = fields[row];
        if (field.number && (x < 176 || x > 268))
        {
            const int next =
                std::clamp(*field.number + (x < 176 ? -field.step : field.step), field.min, field.max);
            edit = field.seconds ? HudStyle::OffsetText(next) : std::to_wstring(next);
            selectAll = false;
            commit(true);
            active = -1;
        }
        else
            selectAll = true;
        SetFocus(window);
        caret = true;
        blink = GetTickCount64();
        paint();
        return 0;
    }
    case WM_MOUSEMOVE:
        if (dragging >= 0)
            slide(GET_X_LPARAM(l) / dpi);
        return 0;
    case WM_LBUTTONUP:
        if (dragging >= 0)
            finishSlide(true);
        return 0;
    case WM_CAPTURECHANGED:
        if (dragging >= 0)
            finishSlide(true);
        return 0;
    case WM_CANCELMODE:
        finishSlide(false);
        return 0;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE)
        {
            hide(false);
            return 0;
        }
        if (w == VK_RETURN)
        {
            if (commit(true) && sliders())
            {
                active = -1;
                paint();
            }
            return 0;
        }
        if (sliders() && active < 0 && (w == VK_LEFT || w == VK_RIGHT || w == VK_HOME || w == VK_END))
        {
            dragging = focusedRow;
            dragStart = current;
            auto &field = fields[focusedRow];
            *field.number = w == VK_HOME ? field.min
                            : w == VK_END
                                ? field.max
                                : std::clamp(*field.number + (w == VK_LEFT ? -1 : 1), field.min, field.max);
            finishSlide(true);
            return 0;
        }
        if (w == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            selectAll = true;
            paint();
            return 0;
        }
        if (w == 'V' && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            SendMessageW(window, WM_PASTE, 0, 0);
            return 0;
        }
        if (w == VK_TAB)
        {
            const int row = active < 0 ? focusedRow : (active + 1) % static_cast<int>(fields.size());
            beginEdit(row);
            return 0;
        }
        if (w == VK_DELETE && active >= 0)
        {
            edit.clear();
            selectAll = false;
            editDeadline = GetTickCount64() + 500;
            paint();
            return 0;
        }
        return 0;
    case WM_PASTE:
        if (active >= 0 && OpenClipboard(window))
        {
            HANDLE content = GetClipboardData(CF_UNICODETEXT);
            if (content)
            {
                auto value = static_cast<const wchar_t *>(GlobalLock(content));
                if (value)
                {
                    const size_t limit = fields[active].color ? 7U : fields[active].seconds ? 8U : 5U;
                    edit.assign(value, wcsnlen_s(value, limit));
                    GlobalUnlock(content);
                    selectAll = false;
                    editDeadline = GetTickCount64() + 500;
                }
            }
            CloseClipboard();
            paint();
        }
        return 0;
    case WM_CHAR:
        if (active >= 0 && !(GetKeyState(VK_CONTROL) & 0x8000))
        {
            const bool color = fields[active].color != nullptr;
            if (w == VK_BACK)
            {
                if (selectAll)
                    edit.clear();
                else if (!edit.empty())
                    edit.pop_back();
            }
            else if ((w >= '0' && w <= '9') ||
                     (color && (w == '#' || (w >= 'a' && w <= 'f') || (w >= 'A' && w <= 'F'))) ||
                     (fields[active].seconds && (w == '+' || w == '-' || w == '.')))
            {
                if (selectAll)
                    edit.clear();
                if (edit.size() < (color ? 7U : fields[active].seconds ? 8U : 5U))
                    edit += static_cast<wchar_t>(w);
            }
            else
                return 0;
            selectAll = false;
            notice.clear();
            editDeadline = GetTickCount64() + 500;
            paint();
        }
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}
