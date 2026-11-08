// MIT. Native layered HUD; every window and worker belongs to Spotify.exe.
#include "NativeCore.h"
#include "NativeHud.h"
#include "fullscreen.h"
#include <objidl.h>
#include <gdiplus.h>
#include "NativeText.h"
#include "NativeFlyout.h"
#include "NativeGlass.h"
#include "NativeFrameClock.h"
using namespace Gdiplus;
struct LyricLine
{
    double time;
    std::wstring text;
};
struct Hud
{
    HMODULE module{};
    HWND window{};
    NativeFlyout form;
    NativeGlass glass;
    NativeFrameClock frames;
    HudDrawing::RoundedEdges corners;
    double lastPaintMs = 0, lastRenderTime = 0, nextRenderTime = 0, dragInterval = 1.0 / 120;
    bool appearanceUnsaved = false;
    NativeText renderer, uiText;
    std::vector<std::wstring> uiValues;
    int uiWidth = 0, uiHeight = 0, opacityControl = 55;
    uint64_t uiTextPaintCount = 0;
    Color foregroundColor{255, 245, 245, 240}, backgroundColor{};
    std::wstring trackLabel;
    std::vector<std::wstring> lyricText;
    NativeServer server;
    std::wstring spotify, settingsPath, boundsPath, offsetPath;
    Json settings = Json::object(), offsets = {{"version", 1}, {"offsets", Json::object()}},
         snapshot = Json::object();
    std::string trackKey, trackHash, source, status = "Open Spotify and play a song";
    std::vector<LyricLine> lines;
    bool enabled = true, locked = false, hover = false, playing = false, dirty = true;
    bool fullscreen = false, sizingMoving = false;
    bool snapLyrics = false, animateSeek = false;
    bool pointerResizing = false, restoreDragPending = false;
    POINT restoreDragStart{};
    POINT dragStartPointer{}, dragPointer{};
    RECT dragStartBounds{};
    uint64_t glassUpdateCount = 0;
    RECT normalBounds{};
    int x = 100, y = 40, width = 770, height = 223, fontSize = 22, antialias = 50, delay = 0, index = -1;
    double position = 0, duration = 0, alpha = 0, scroll = 0;
    ULONGLONG sampled = 0, lastPoll = 0;
    bool painted = false;
    DWORD paintError = 0;
    uint64_t frameCount = 0;
    HDC canvas = nullptr;
    HBITMAP raster = nullptr, previousRaster = nullptr;
    void *rasterPixels = nullptr;
    int rasterWidth = 0, rasterHeight = 0;
    int glassWidth = 0, glassHeight = 0, glassOpacity = -1;
    std::vector<double> emphasis;
    std::vector<double> lyricCenters;
    uint64_t lyricLayoutRevision = 0;
    int firstVisible = 0, lastVisible = 0;
    double pulse = 0;
    void releaseCanvas()
    {
        if (canvas)
        {
            SelectObject(canvas, previousRaster);
            DeleteObject(raster);
            DeleteDC(canvas);
        }
        canvas = nullptr;
        raster = previousRaster = nullptr;
        rasterPixels = nullptr;
        rasterWidth = rasterHeight = 0;
    }
    bool saveSettings()
    {
        settings["fontSize"] = fontSize;
        settings["hudEnabled"] = enabled;
        settings["antialias"] = antialias;
        dirty = true;
        return WriteJson(settingsPath, settings);
    }
    void updateGlass()
    {
        if (!window)
            return;
        const int opacity = opacityControl;
        // No compositor is needed until a visible, translucent background is requested.
        if ((enabled || alpha > 0) && opacity > 0 && opacity < 100)
            glass.attach(window);
        if (glassWidth == width && glassHeight == height && glassOpacity == opacity)
            return;
        const bool initial = !glassWidth;
        const bool blurChanged = glassOpacity != opacity;
        // Frame configuration can synchronously reenter WM_WINDOWPOSCHANGED.
        glassWidth = width;
        glassHeight = height;
        glassOpacity = opacity;
        if (initial)
            ApplyHudFrame(window);
        if (initial || blurChanged)
            ++glassUpdateCount;
        glass.sync(16, opacity, alpha);
    }
    void beginSizeMove(bool resizing, POINT pointer)
    {
        if (sizingMoving)
            return;
        form.hide();
        sizingMoving = true;
        pointerResizing = resizing;
        ClientToScreen(window, &pointer);
        dragStartPointer = dragPointer = pointer;
        dragStartBounds = {x, y, x + width, y + height};
        MONITORINFOEXW monitor{};
        monitor.cbSize = sizeof(monitor);
        DEVMODEW display{};
        display.dmSize = sizeof(display);
        int refresh = 120;
        if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor) &&
            EnumDisplaySettingsW(monitor.szDevice, ENUM_CURRENT_SETTINGS, &display) &&
            display.dmDisplayFrequency > 1)
            refresh = static_cast<int>(display.dmDisplayFrequency);
        dragInterval = 1.0 / std::clamp(refresh, 60, 240);
        // Do not enter User32's synchronous modal drag loop. Keep blur enabled.
        // Mouse reports replace the latest target; frame deadlines commit geometry.
        SetForegroundWindow(window);
        if (GetCapture() != window)
            SetCapture(window);
        frames.request();
    }
    void beginRestoreDrag(POINT pointer)
    {
        form.hide();
        ClientToScreen(window, &pointer);
        restoreDragStart = pointer;
        restoreDragPending = true;
        SetCapture(window);
    }
    void moveRestoreDrag(LPARAM point)
    {
        POINT pointer{static_cast<short>(LOWORD(point)), static_cast<short>(HIWORD(point))};
        ClientToScreen(window, &pointer);
        if (abs(pointer.x - restoreDragStart.x) < GetSystemMetrics(SM_CXDRAG) &&
            abs(pointer.y - restoreDragStart.y) < GetSystemMetrics(SM_CYDRAG))
            return;
        const double grab = std::clamp((restoreDragStart.x - x) / static_cast<double>(width), 0.0, 1.0);
        const int grabY = std::clamp<int>(restoreDragStart.y - y, 0, 24);
        const int restoredWidth = normalBounds.right - normalBounds.left;
        const int restoredHeight = normalBounds.bottom - normalBounds.top;
        restoreDragPending = false;
        fullscreen = false;
        SetWindowLongPtrW(window, GWL_EXSTYLE, GetWindowLongPtrW(window, GWL_EXSTYLE) | WS_EX_NOACTIVATE);
        SetWindowPos(window, HWND_TOPMOST, pointer.x - static_cast<int>(restoredWidth * grab),
                     pointer.y - grabY, restoredWidth, restoredHeight, SWP_NOACTIVATE);
        ScreenToClient(window, &pointer);
        beginSizeMove(false, pointer);
        dirty = true;
        frames.request();
    }
    void movePointer(LPARAM point)
    {
        dragPointer = {static_cast<short>(LOWORD(point)), static_cast<short>(HIWORD(point))};
        ClientToScreen(window, &dragPointer);
    }
    void applyDrag()
    {
        const int dx = dragPointer.x - dragStartPointer.x, dy = dragPointer.y - dragStartPointer.y;
        const int nextX = dragStartBounds.left + (pointerResizing ? 0 : dx);
        const int nextY = dragStartBounds.top + (pointerResizing ? 0 : dy);
        const int nextWidth = std::clamp<int>(
            dragStartBounds.right - dragStartBounds.left + (pointerResizing ? dx : 0), 420, 2000);
        const int nextHeight = std::clamp<int>(
            dragStartBounds.bottom - dragStartBounds.top + (pointerResizing ? dy : 0), 110, 1000);
        if (nextX != x || nextY != y || nextWidth != width || nextHeight != height)
            SetWindowPos(window, nullptr, nextX, nextY, nextWidth, nextHeight, SWP_NOACTIVATE | SWP_NOZORDER);
    }
    void endSizeMove()
    {
        if (restoreDragPending)
        {
            restoreDragPending = false;
            if (GetCapture() == window)
                ReleaseCapture();
        }
        if (!sizingMoving)
            return;
        sizingMoving = false;
        if (GetCapture() == window)
            ReleaseCapture();
        updateGlass();
        dirty = true;
        frames.request();
        saveBounds();
    }
    void saveBounds()
    {
        if (!fullscreen)
            WriteJson(boundsPath, {{"x", x}, {"y", y}, {"width", width}, {"height", height}});
    }
    RECT flyoutArea() const
    {
        return {x, y, x + width, y + (fullscreen ? 32 : height)};
    }
    void toggleFullscreen()
    {
        endSizeMove();
        form.hide();
        RECT target{};
        if (!fullscreen)
        {
            normalBounds = {x, y, x + width, y + height};
            MONITORINFO monitor{sizeof(monitor)};
            if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
                return;
            target = monitor.rcMonitor;
        }
        else
        {
            target = normalBounds;
            if (!MonitorFromRect(&target, MONITOR_DEFAULTTONULL))
            {
                MONITORINFO monitor{sizeof(monitor)};
                if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
                    OffsetRect(&target, monitor.rcWork.left - target.left, monitor.rcWork.top - target.top);
            }
        }
        fullscreen = !fullscreen;
        const auto style = GetWindowLongPtrW(window, GWL_EXSTYLE);
        SetWindowLongPtrW(window, GWL_EXSTYLE, fullscreen ? style & ~WS_EX_NOACTIVATE : style | WS_EX_NOACTIVATE);
        SetWindowPos(window, HWND_TOPMOST, target.left, target.top, target.right - target.left,
                     target.bottom - target.top, SWP_NOACTIVATE);
        if (fullscreen)
        {
            SetForegroundWindow(window);
            SetFocus(window);
        }
        dirty = true;
        frames.request();
    }
    double progress() const
    {
        return std::clamp(position + (playing ? static_cast<double>(GetTickCount64() - sampled) / 1000 : 0.0),
                          0.0, std::max(0.0, duration));
    }
    void health()
    {
        DWORD actualPid = 0;
        GetWindowThreadProcessId(window, &actualPid);
        RECT popup{};
        if (form.window)
            GetWindowRect(form.window, &popup);
        Json j = {{"version", HudVersion},
                  {"uiRevision", "lightweight-glass"},
                  {"renderSurfaceBytes", static_cast<uint64_t>(rasterWidth) * rasterHeight * 4 +
                                             renderer.surfaceBytes() + uiText.surfaceBytes()},
                  {"glassStrength", glass.strength()},
                  {"glassHwnd", reinterpret_cast<uintptr_t>(glass.window())},
                  {"glassError", static_cast<uint32_t>(glass.error())},
                  {"opacity", glassOpacity},
                  {"timingText", Utf8(HudStyle::OffsetText(delay))},
                  {"flyout",
                   {{"kind", static_cast<int>(form.kind)},
                    {"opacity", form.opacity()},
                    {"x", popup.left},
                    {"y", popup.top},
                    {"width", popup.right - popup.left},
                    {"height", popup.bottom - popup.top}}},
                  {"integration", "spotify-native"},
                  {"pid", GetCurrentProcessId()},
                  {"windowPid", actualPid},
                  {"hwnd", reinterpret_cast<uintptr_t>(window)},
                  {"enabled", enabled},
                  {"visible", IsWindowVisible(window) != FALSE},
                  {"fullscreenLocked", locked},
                  {"fullscreen", fullscreen},
                  {"foreground", GetForegroundWindow() == window},
                  {"keyboardFocus", GetFocus() == window},
                  {"sizingMoving", sizingMoving},
                  {"glassUpdateCount", glassUpdateCount},
                  {"uiTextPaintCount", uiTextPaintCount},
                  {"uiSurfaceHeight", uiHeight},
                  {"title", snapshot.value("title", std::string())},
                  {"artist", snapshot.value("artist", std::string())},
                  {"lineCount", lines.size()},
                  {"source", source},
                  {"positionSec", progress()},
                  {"offsetMs", delay},
                  {"activeIndex", index},
                  {"scrollIndex", scroll},
                  {"activeEmphasis", index >= 0 && index < static_cast<int>(emphasis.size()) ? emphasis[index] : 0},
                  {"activeWrappedRows", index >= 0 ? renderer.rowCount(index) : 0},
                  {"activeTextHeight", index >= 0 ? renderer.textHeight(index) : 0},
                  {"activeFontSize", index >= 0 ? renderer.fittedSize(index) : 0},
                  {"lyricLayoutCount", renderer.revision()},
                  {"visibleLyricBlocks", lastVisible - firstVisible},
                  {"painted", painted},
                  {"paintError", paintError},
                  {"frameCount", frameCount},
                  {"frameTimeSec", NativeFrameClock::now()},
                  {"lastPaintMs", lastPaintMs},
                  {"alpha", alpha},
                  {"antialias", antialias},
                  {"textRenderer", "DirectWrite grayscale"},
                  {"flyoutHwnd", reinterpret_cast<uintptr_t>(form.window)},
                  {"bounds", {{"x", x}, {"y", y}, {"width", width}, {"height", height}}},
                  {"exStyle", GetWindowLongPtrW(window, GWL_EXSTYLE)}};
        std::lock_guard<std::mutex> guard(server.healthMutex);
        server.health = j.dump();
    }
    void publish()
    {
        server.broadcast({{"type", "state"}, {"enabled", enabled}});
        health();
    }
    void setEnabled(bool value)
    {
        enabled = value;
        if (!value)
        {
            endSizeMove();
            form.hide();
            if (fullscreen)
                toggleFullscreen();
        }
        saveSettings();
        if (value)
        {
            ShowWindow(window, SW_SHOWNOACTIVATE);
            frames.request();
        }
        publish();
    }
    bool setDelay(int milliseconds)
    {
        if (trackHash.empty())
            return false;
        const int next = std::clamp(milliseconds, -60000, 60000);
        if (next == delay)
            return true;
        Json copy = offsets;
        if (next)
            copy["offsets"][trackHash] = {{"title", snapshot.value("title", std::string())},
                                          {"artist", snapshot.value("artist", std::string())},
                                          {"durationSec", duration},
                                          {"delayMs", next}};
        else
            copy["offsets"].erase(trackHash);
        if (WriteJson(offsetPath, copy))
        {
            offsets = std::move(copy);
            delay = next;
            dirty = true;
            return true;
        }
        return false;
    }
    void message(const Json &m)
    {
        const auto type = m.value("type", std::string());
        if (type == "connected")
        {
            publish();
            server.broadcast({{"type", "getPlayback"}});
        }
        else if (type == "setEnabled" && m.contains("enabled") && m["enabled"].is_boolean())
            setEnabled(m["enabled"].get<bool>());
        else if (type == "playback" && m.contains("snapshot") && m["snapshot"].is_object())
        {
            auto s = m["snapshot"];
            if (!s.value("found", false))
            {
                if (trackKey.empty() && status == "No track playing" && !playing)
                    return;
                dirty = true;
                if (form.kind == FlyoutKind::Timing)
                    form.hide(false);
                playing = false;
                snapshot = Json::object();
                trackKey.clear();
                trackHash.clear();
                trackLabel.clear();
                lines.clear();
                lyricText.clear();
                emphasis.clear();
                lyricCenters.clear();
                firstVisible = lastVisible = 0;
                renderer.clearLayouts();
                index = -1;
                scroll = 0;
                snapLyrics = animateSeek = false;
                status = "No track playing";
                return;
            }
            auto title = s.value("title", std::string()), artist = s.value("artist", std::string());
            if (title.size() > 2048 || artist.size() > 2048)
                return;
            double d = s.value("durationSec", 0.0), p = s.value("positionSec", 0.0);
            if (!std::isfinite(d) || !std::isfinite(p) || d < 0 || d > 86400 || p < 0 || p > d + 5)
                return;
            auto key = Json::array({title, artist, static_cast<int>(std::round(d))}).dump();
            const bool nextPlaying = s.value("isPlaying", false);
            const bool seek = s.value("seek", false);
            if (key == trackKey)
            {
                if (seek)
                {
                    // Update the clock immediately, then ease toward the latest requested row.
                    animateSeek = true;
                    snapLyrics = false;
                }
                else if (std::abs(p - progress()) > 0.5)
                {
                    snapLyrics = true;
                    animateSeek = false;
                }
            }
            dirty = dirty || key != trackKey || playing != nextPlaying || duration != d ||
                    (!nextPlaying && position != p) || snapLyrics || seek;
            if (key != trackKey)
            {
                trackLabel = Wide(title + "  ·  " + artist);
                if (form.kind == FlyoutKind::Timing)
                    form.hide(false);
                trackKey = key;
                trackHash = HexHash(BCRYPT_SHA256_ALGORITHM, key);
                lines.clear();
                lyricText.clear();
                emphasis.clear();
                lyricCenters.clear();
                firstVisible = lastVisible = 0;
                renderer.clearLayouts();
                index = -1;
                scroll = 0;
                snapLyrics = animateSeek = false;
                delay = 0;
                source.clear();
                status = "Searching lyrics...";
                if (offsets["offsets"].contains(trackHash))
                {
                    const auto &entry = offsets["offsets"][trackHash];
                    if (entry.is_object() && entry.contains("delayMs") &&
                        entry["delayMs"].is_number_integer())
                        delay = std::clamp(entry["delayMs"].get<int>(), -60000, 60000);
                }
            }
            snapshot = std::move(s);
            position = p;
            duration = d;
            playing = nextPlaying;
            sampled = GetTickCount64();
        }
        else if (type == "lyrics" && m.value("key", std::string()) == trackKey && !trackKey.empty())
        {
            if (!m.contains("lines") || !m["lines"].is_array() || m["lines"].size() > 10000)
                return;
            std::vector<LyricLine> next;
            double last = -1;
            for (const auto &row : m["lines"])
            {
                double time = row.at("time").get<double>();
                auto text = row.at("text").get<std::string>();
                if (!std::isfinite(time) || time < 0 || time < last || time > 86400 || text.size() > 8192)
                    return;
                next.push_back({time, Wide(text)});
                last = time;
            }
            lines = std::move(next);
            dirty = true;
            lyricText.clear();
            for (const auto &line : lines)
                lyricText.push_back(line.text);
            renderer.invalidate();
            emphasis.assign(lines.size(), 0);
            firstVisible = lastVisible = 0;
            source = m.value("source", std::string());
            status = lines.empty() ? "No synced lyrics found" : "";
            index = -1;
        }
    }
    Color color(const char *key, const char *fallback, int opacity = 255)
    {
        auto hex = settings.value(key, std::string(fallback));
        unsigned int rgb = 0;
        if (hex.size() != 7 || hex[0] != '#' || sscanf_s(hex.c_str() + 1, "%x", &rgb) != 1)
            sscanf_s(fallback + 1, "%x", &rgb);
        return Color(static_cast<BYTE>(opacity), static_cast<BYTE>(rgb >> 16), static_cast<BYTE>(rgb >> 8),
                     static_cast<BYTE>(rgb));
    }
    void refreshStyle()
    {
        opacityControl =
            std::clamp(static_cast<int>(std::round(settings.value("transparency", 0.55) * 100)), 0, 100);
        foregroundColor = color("lyricColor", "#f5f5f0");
        backgroundColor =
            HudStyle::GlassTint(Wide(settings.value("bgColor", std::string("#121214"))), opacityControl);
    }
    void paint(double dt)
    {
        updateGlass();
        if (!enabled && alpha <= 0)
        {
            ShowWindow(window, SW_HIDE);
            return;
        }
        const double target = enabled ? 1 : 0;
        alpha += std::clamp(target - alpha, -dt / 0.11, dt / 0.11);
        alpha = std::clamp(alpha, 0.0, 1.0);
        dirty = false;
        ++frameCount;
        HDC screen = GetDC(nullptr);
        if (!canvas || rasterWidth != width || rasterHeight != height)
        {
            releaseCanvas();
            canvas = CreateCompatibleDC(screen);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            raster = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &rasterPixels, nullptr, 0);
            if (!raster)
            {
                DeleteDC(canvas);
                canvas = nullptr;
                ReleaseDC(nullptr, screen);
                return;
            }
            previousRaster = static_cast<HBITMAP>(SelectObject(canvas, raster));
            rasterWidth = width;
            rasterHeight = height;
        }
        {
            Bitmap surface(width, height, width * 4, PixelFormat32bppPARGB,
                           static_cast<BYTE *>(rasterPixels));
            Graphics g(&surface);
            // Fill once, then mask the final premultiplied surface at its edge.
            g.Clear(HudStyle::InteractiveTint(backgroundColor));
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
            float w = static_cast<float>(width), h = static_cast<float>(height);
            HudDrawing::Rounded(g, RectF(0.5f, 0.5f, w - 1, h - 1), 15.5f, Color(0, 0, 0, 0),
                                Color(static_cast<BYTE>(std::round(40 * HudStyle::GlassOpacity(opacityControl))),
                                      255, 255, 255));
            const float top = 32, bottom = h, center = (top + bottom) / 2;
            auto p = progress() - delay / 1000.0;
            int next = -1;
            if (!lines.empty())
            {
                auto it = std::upper_bound(lines.begin(), lines.end(), p,
                                           [](double t, const LyricLine &l) { return t < l.time; });
                next = static_cast<int>(it - lines.begin()) - 1;
            }
            if (next != index)
            {
                index = next;
            }
            double dest = std::max(0, index);
            const bool snap = snapLyrics;
            snapLyrics = false;
            if (snap || (!animateSeek && std::abs(scroll - dest) > 6))
                scroll = dest;
            else
                scroll += (dest - scroll) * (1 - std::exp(-dt * 15));
            if (std::abs(scroll - dest) <= 0.01)
            {
                scroll = dest;
                animateSeek = false;
            }
            if (playing)
                pulse += dt;
            if (!lines.empty() &&
                renderer.prepare(lyricText, fontSize, width - 48,
                                 std::max(1, static_cast<int>((h - top) * 0.64f - 12))) &&
                renderer.begin(width, height, top, antialias))
            {
                if (lyricLayoutRevision != renderer.revision())
                {
                    lyricCenters.resize(lines.size());
                    lyricCenters[0] = 0;
                    for (size_t i = 1; i < lines.size(); ++i)
                        lyricCenters[i] = lyricCenters[i - 1] +
                                          (renderer.textHeight(i - 1) + renderer.textHeight(i)) / 2 + 16;
                    lyricLayoutRevision = renderer.revision();
                }
                if (emphasis.size() != lines.size())
                    emphasis.assign(lines.size(), 0);
                const size_t anchor = std::min(static_cast<size_t>(scroll), lyricCenters.size() - 1);
                double offset = lyricCenters[anchor];
                if (anchor + 1 < lyricCenters.size())
                    offset += (lyricCenters[anchor + 1] - offset) * (scroll - anchor);
                // Search cached block centers, rather than imposing a fixed number of visible lines.
                const int begin = std::max(
                    0,
                    static_cast<int>(std::lower_bound(lyricCenters.begin(), lyricCenters.end(), offset - h) -
                                     lyricCenters.begin()) -
                        1);
                const int end = std::min(
                    static_cast<int>(lines.size()),
                    static_cast<int>(std::upper_bound(lyricCenters.begin(), lyricCenters.end(), offset + h) -
                                     lyricCenters.begin()) +
                        1);
                for (int i = firstVisible; i < lastVisible; ++i)
                    if (i < begin || i >= end)
                        emphasis[i] = 0;
                firstVisible = begin;
                lastVisible = end;
                for (int i = begin; i < end; ++i)
                {
                    const float ly = center + static_cast<float>(lyricCenters[i] - offset);
                    const bool active = i == index;
                    if (snap)
                        emphasis[i] = active ? 1.0 : 0.0;
                    else
                        emphasis[i] += ((active ? 1.0 : 0.0) - emphasis[i]) * (1 - std::exp(-dt * 26));
                    const float scale = 0.68f + 0.32f * static_cast<float>(emphasis[i]);
                    const float tx = (w - renderer.layoutWidth(i) * scale) / 2;
                    renderer.draw(i, tx, ly, scale,
                                  Color(static_cast<BYTE>(96 + 159 * emphasis[i]), foregroundColor.GetR(),
                                        foregroundColor.GetG(), foregroundColor.GetB()));
                    if (active)
                    {
                        const double breath = (1 - std::cos(pulse * 6.28318530718 / 1.6)) / 2;
                        renderer.underline(w / 2, ly + renderer.textHeight(i) * scale / 2 + 4,
                                           36.0f * static_cast<float>(0.85 + 0.15 * breath), breath);
                    }
                }
                if (!renderer.composite(g))
                    dirty = true;
            }
            struct Label
            {
                std::wstring value;
                float x, y, size, maxWidth;
                Color color;
                bool center;
            };
            std::vector<Label> labels;
            labels.reserve(4);
            if (lines.empty())
                labels.push_back({Wide(status), w / 2, center, 14, w - 36, Color(160, 245, 245, 240), true});
            if ((hover || form.window) && !locked)
            {
                const auto first = HudStyle::ToolbarBox(width, 0);
                labels.push_back({trackLabel, 14, 16, HudStyle::UiFontSize, std::max(1.0f, first.X - 28),
                                  Color(170, 245, 245, 240), false});
                for (int i = 0; i < HudStyle::ToolbarCount; ++i)
                {
                    const auto box = HudStyle::ToolbarBox(width, i);
                    const bool selected =
                        (form.window && i < 4 && static_cast<int>(form.kind) == i) || (i == 4 && fullscreen);
                    HudDrawing::Rounded(g, box, 6,
                                        selected ? Color(50, 53, 208, 127) : Color(25, 255, 255, 255));
                    if (i < 2)
                        labels.push_back({i == 0 ? HudStyle::OffsetText(delay) : L"Aa", box.X + box.Width / 2,
                                          box.Y + box.Height / 2, HudStyle::UiFontSize, box.Width - 8,
                                          Color(240, 245, 245, 240), true});
                    else
                    {
                        const HudDrawing::Icon icons[] = {HudDrawing::Icon::Opacity, HudDrawing::Icon::Hash,
                                                          fullscreen ? HudDrawing::Icon::Restore
                                                                     : HudDrawing::Icon::Fullscreen,
                                                          HudDrawing::Icon::Close};
                        HudDrawing::DrawIcon(g, icons[i - 2],
                                             RectF(box.X + (box.Width - 18) / 2, box.Y + 3, 18, 18),
                                             Color(230, 245, 245, 240));
                    }
                }
            }
            if (!labels.empty())
            {
                std::vector<std::wstring> values;
                values.reserve(labels.size());
                for (const auto &label : labels)
                    values.push_back(label.value);
                const int textHeight = lines.empty() ? height : 32;
                const bool repaintText = values != uiValues || uiWidth != width || uiHeight != textHeight;
                if (values != uiValues)
                {
                    uiValues = std::move(values);
                    uiText.invalidate();
                }
                if (!repaintText && uiText.compositeCached(g))
                {
                    // Stable toolbar text is already rasterized; only lyrics animate.
                }
                else if (uiText.prepare(uiValues, HudStyle::UiFontSize) &&
                         uiText.begin(width, textHeight, 0, 50, false))
                {
                    ++uiTextPaintCount;
                    for (size_t i = 0; i < labels.size(); ++i)
                    {
                        const auto &label = labels[i];
                        float scale = label.size / HudStyle::UiFontSize;
                        if (label.center && label.value == HudStyle::OffsetText(delay))
                            scale = std::min(scale, label.maxWidth / std::max(1.0f, uiText.textWidth(i)));
                        const float left =
                            label.center ? label.x - std::min(label.maxWidth, uiText.textWidth(i) * scale) / 2
                                         : label.x;
                        uiText.draw(i, left, label.y, scale, label.color, label.maxWidth, true);
                    }
                    if (!uiText.composite(g))
                        dirty = true;
                    else
                    {
                        uiWidth = width;
                        uiHeight = textHeight;
                    }
                }
            }
        }
        corners.apply(rasterPixels, width, height, 16);
        POINT at{x, y}, origin{};
        SIZE size{width, height};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, static_cast<BYTE>(std::round(alpha * 255)), AC_SRC_ALPHA};
        painted = !!UpdateLayeredWindow(window, screen, &at, &size, canvas, &origin, 0, &blend, ULW_ALPHA);
        paintError = painted ? 0 : GetLastError();
        ReleaseDC(nullptr, screen);
        glass.sync(16, opacityControl, alpha);
    }
    double tick()
    {
        const double frameNow = NativeFrameClock::now();
        const auto now = GetTickCount64();
        const double dt = lastRenderTime ? std::min(0.05, frameNow - lastRenderTime) : 1.0 / 60;
        if (sizingMoving)
            applyDrag();
        if (now - lastPoll >= 250)
        {
            lastPoll = now;
            HWND front = GetForegroundWindow();
            bool value = front != window && front != form.window && IsFullscreenWindow(front, 0);
            if (value != locked)
            {
                locked = value;
                if (locked)
                {
                    endSizeMove();
                    form.hide();
                }
                hover = false;
                dirty = true;
                LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
                style = locked ? style | WS_EX_TRANSPARENT : style & ~WS_EX_TRANSPARENT;
                SetWindowLongPtrW(window, GWL_EXSTYLE, style);
                SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            }
            health();
        }
        auto animating = [&] {
            if (std::abs(alpha - (enabled ? 1.0 : 0.0)) > 0.001 ||
                std::abs(scroll - std::max(0, index)) > 0.01)
                return true;
            for (int i = firstVisible; i < lastVisible; ++i)
                if (std::abs(emphasis[i] - (i == index ? 1.0 : 0.0)) > 0.001)
                    return true;
            return false;
        };
        const bool motion = (playing && !lines.empty()) || animating();
        if ((enabled || alpha > 0) && (dirty || motion) &&
            (!sizingMoving || frameNow >= nextRenderTime - 0.0001))
        {
            lastRenderTime = frameNow;
            nextRenderTime = sizingMoving ? nextRenderTime + 1.0 / 60 : frameNow + 1.0 / 60;
            if (nextRenderTime <= frameNow)
                nextRenderTime = frameNow + 1.0 / 60;
            const double start = NativeFrameClock::now();
            paint(dt);
            lastPaintMs = (NativeFrameClock::now() - start) * 1000;
        }
        if (!enabled && alpha <= 0)
        {
            ShowWindow(window, SW_HIDE);
            glass.sync(16, opacityControl, 0);
            // Fullscreen rasters can otherwise retain tens of MiB after closing the HUD.
            // Keep layouts, so reopening does not remeasure every lyric.
            releaseCanvas();
            renderer.releaseSurface();
            uiText.releaseSurface();
            return 0.25;
        }
        if (sizingMoving)
            return dragInterval;
        return (enabled || alpha > 0) && (motion || dirty) ? 1.0 / 60 : 0.1;
    }
    void previewAppearance(const HudAppearance &next)
    {
        settings["fontSize"] = fontSize = next.fontSize;
        settings["antialias"] = antialias = next.antialias;
        settings["transparency"] = next.opacity / 100.0;
        settings["bgColor"] = Utf8(next.background);
        settings["lyricColor"] = Utf8(next.foreground);
        refreshStyle();
        appearanceUnsaved = true;
        dirty = true;
        updateGlass();
        frames.request();
    }
    void openFlyout(FlyoutKind kind)
    {
        if (form.window && form.kind == kind)
        {
            form.hide();
            dirty = true;
            return;
        }
        form.hide();
        HudAppearance value;
        value.fontSize = fontSize;
        value.offsetMs = delay;
        value.opacity = opacityControl;
        value.antialias = antialias;
        value.background = Wide(settings.value("bgColor", std::string("#121214")));
        value.foreground = Wide(settings.value("lyricColor", std::string("#f5f5f0")));
        RECT area = flyoutArea();
        const int anchor = x + HudStyle::ToolbarAnchor(width, static_cast<int>(kind));
        form.show(
            module, window, area, anchor, kind, value,
            [this, kind](const HudAppearance &next, std::wstring &error) {
                if (kind == FlyoutKind::Timing)
                {
                    if (!setDelay(next.offsetMs))
                    {
                        error = L"Could not save lyric timing";
                        return false;
                    }
                    frames.request();
                    return true;
                }
                Json saved = settings;
                saved["fontSize"] = next.fontSize;
                saved["antialias"] = next.antialias;
                saved["transparency"] = next.opacity / 100.0;
                saved["bgColor"] = Utf8(next.background);
                saved["lyricColor"] = Utf8(next.foreground);
                const bool changed = saved != settings;
                if ((changed || appearanceUnsaved) && !WriteJson(settingsPath, saved))
                {
                    error = L"Could not save settings";
                    return false;
                }
                settings = std::move(saved);
                appearanceUnsaved = false;
                fontSize = next.fontSize;
                antialias = next.antialias;
                refreshStyle();
                dirty = true;
                updateGlass();
                frames.request();
                return true;
            },
            [this](const HudAppearance &next) { previewAppearance(next); });
        dirty = true;
    }
};
static Hud *hud = nullptr;
static LRESULT CALLBACK HudProc(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (!hud)
        return DefWindowProcW(window, message, w, l);
    try
    {
        switch (message)
        {
        case HudStyle::AppearanceChanged:
            hud->dirty = true;
            hud->frames.request();
            return 0;
        case HudMessage: {
            std::unique_ptr<Json> data(reinterpret_cast<Json *>(l));
            if (data)
                hud->message(*data);
            hud->frames.request();
            return 0;
        }
        case WM_TIMER:
            if (w == 2)
                hud->server.broadcast({{"type", "getPlayback"}});
            return 0;
        case WM_MOUSEACTIVATE:
            return hud->fullscreen && !hud->locked ? MA_ACTIVATE : MA_NOACTIVATE;
        case WM_MOUSEMOVE:
            if (hud->restoreDragPending)
            {
                hud->moveRestoreDrag(l);
                return 0;
            }
            if (hud->sizingMoving)
            {
                hud->movePointer(l);
                return 0;
            }
            if (!hud->locked && !hud->hover)
            {
                hud->hover = true;
                hud->dirty = true;
                hud->frames.request();
                TRACKMOUSEEVENT e{sizeof(e), TME_LEAVE, window, 0};
                TrackMouseEvent(&e);
            }
            return 0;
        case WM_MOUSELEAVE:
            hud->hover = false;
            hud->dirty = true;
            return 0;
        case WM_NCHITTEST: {
            if (hud->locked)
                return HTTRANSPARENT;
            return HTCLIENT;
        }
        case WM_SETCURSOR: {
            POINT pointer{};
            GetCursorPos(&pointer);
            ScreenToClient(window, &pointer);
            if (!hud->locked && !hud->fullscreen &&
                ((hud->sizingMoving && hud->pointerResizing) ||
                 (pointer.x >= hud->width - 12 && pointer.y >= hud->height - 12)))
            {
                SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE));
                return TRUE;
            }
            break;
        }
        case WM_LBUTTONDOWN:
            if (hud->locked)
                return 0;
            {
                int px = static_cast<short>(LOWORD(l)), py = static_cast<short>(HIWORD(l));
                if (!hud->fullscreen && px >= hud->width - 12 && py >= hud->height - 12)
                {
                    hud->beginSizeMove(true, {px, py});
                    return 0;
                }
                int button = -1;
                for (int i = 0; i < HudStyle::ToolbarCount; ++i)
                    if (HudStyle::ToolbarBox(hud->width, i)
                            .Contains(static_cast<float>(px), static_cast<float>(py)))
                        button = i;
                if (button >= 0)
                {
                    if (button < 4)
                        hud->openFlyout(static_cast<FlyoutKind>(button));
                    else if (button == 4)
                        hud->toggleFullscreen();
                    else
                        hud->setEnabled(false);
                }
                else
                {
                    hud->form.hide();
                    if (hud->fullscreen)
                        SetFocus(window);
                    if (!hud->fullscreen)
                        hud->beginSizeMove(false, {px, py});
                    else if (py >= 0 && py < 28)
                        hud->beginRestoreDrag({px, py});
                }
                return 0;
            }
        case WM_LBUTTONUP:
            if (hud->restoreDragPending)
                hud->endSizeMove();
            if (hud->sizingMoving)
            {
                hud->movePointer(l);
                hud->applyDrag();
                hud->endSizeMove();
            }
            return 0;
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
            hud->endSizeMove();
            return 0;
        case WM_KEYDOWN:
            if (w == VK_ESCAPE && hud->restoreDragPending)
                hud->endSizeMove();
            else if (w == VK_ESCAPE && hud->sizingMoving)
            {
                hud->dragPointer = hud->dragStartPointer;
                hud->applyDrag();
                hud->endSizeMove();
            }
            // User32 routes keyboard input to the focused window; no global hook is installed.
            else if (hud->fullscreen && !hud->locked && GetFocus() == window &&
                     !(GetKeyState(VK_CONTROL) & 0x8000) &&
                     !(GetKeyState(VK_MENU) & 0x8000))
            {
                if (w == VK_LEFT || w == VK_RIGHT)
                    hud->server.broadcast({{"type", "playerAction"}, {"action", "seek"},
                                           {"deltaMs", w == VK_LEFT ? -5000 : 5000}});
                else if (w == VK_UP || w == VK_DOWN)
                    hud->server.broadcast({{"type", "playerAction"}, {"action", "volume"},
                                           {"delta", w == VK_UP ? 0.05 : -0.05}});
                else if (w == VK_SPACE && !(l & (1LL << 30)))
                    hud->server.broadcast({{"type", "playerAction"}, {"action", "toggle"}});
            }
            return 0;
        case WM_RBUTTONUP:
            if (!hud->locked)
                hud->openFlyout(FlyoutKind::Typography);
            return 0;
        case WM_GETMINMAXINFO: {
            if (hud->fullscreen)
                return DefWindowProcW(window, message, w, l);
            auto p = reinterpret_cast<MINMAXINFO *>(l);
            p->ptMinTrackSize = {420, 110};
            p->ptMaxTrackSize = {2000, 1000};
            return 0;
        }
        case WM_WINDOWPOSCHANGED: {
            RECT r;
            if (GetWindowRect(window, &r))
            {
                const bool resized = hud->width != r.right - r.left || hud->height != r.bottom - r.top;
                hud->x = r.left;
                hud->y = r.top;
                hud->width = r.right - r.left;
                hud->height = r.bottom - r.top;
                // Moving a layered window already moves its cached surface in DWM.
                // Do not redraw it at mouse report frequency just because its origin changed.
                hud->dirty = hud->dirty || resized;
                if (hud->form.window)
                    hud->form.reposition(
                        hud->flyoutArea(),
                        hud->x + HudStyle::ToolbarAnchor(hud->width, static_cast<int>(hud->form.kind)));
                if (!hud->sizingMoving)
                    hud->updateGlass();
                hud->glass.sync(16, hud->opacityControl, hud->alpha);
            }
            return 0;
        }
        case WM_CLOSE:
            hud->setEnabled(false);
            return 0;
        case WM_DISPLAYCHANGE:
            if (hud->fullscreen)
                hud->toggleFullscreen();
            return 0;
        }
    }
    catch (...)
    { /* Untrusted transport or paint errors must not escape a window procedure. */
    }
    return DefWindowProcW(window, message, w, l);
}
void RunNativeHud(HMODULE module, const std::wstring &spotify, const std::wstring &root)
{
    auto config = ReadJson(root + L"\\connection.json");
    if (!config.is_object())
        return;
    auto token = config.value("token", std::string());
    if (token.size() != 64 || token.find_first_not_of("0123456789abcdef") != std::string::npos)
        return;
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    GdiplusStartupInput input;
    ULONG_PTR graphicsToken = 0;
    if (GdiplusStartup(&graphicsToken, &input, nullptr) != Ok)
        return;
    // Lives for the host lifetime: detached network workers never reference freed state.
    hud = new Hud();
    hud->module = module;
    hud->spotify = spotify;
    hud->offsetPath = spotify + L"\\LyricsHUD\\lyric-offsets.json";
    wchar_t appdata[32768]{};
    GetEnvironmentVariableW(L"APPDATA", appdata, ARRAYSIZE(appdata));
    std::wstring state = std::wstring(appdata) + L"\\spotify-lyrics-hud";
    hud->settingsPath = state + L"\\settings.json";
    hud->boundsPath = state + L"\\window-state.json";
    auto settings = ReadJson(hud->settingsPath);
    if (settings.is_object())
        hud->settings = settings;
    if (hud->settings.contains("transparency") && hud->settings.value("opacityCurve", 1) != 2)
    {
        hud->settings["transparency"] =
            HudStyle::OpacityControl(hud->settings.value("transparency", 0.55)) / 100.0;
        hud->settings["opacityCurve"] = 2;
        WriteJson(hud->settingsPath, hud->settings);
    }
    hud->settings["opacityCurve"] = 2;
    auto offsets = ReadJson(hud->offsetPath);
    if (offsets.is_object() && offsets.contains("offsets") && offsets["offsets"].is_object())
        hud->offsets = offsets;
    hud->enabled = hud->settings.value("hudEnabled", true);
    hud->fontSize = std::clamp(hud->settings.value("fontSize", 22), 14, HudStyle::MaxFontSize);
    hud->antialias = std::clamp(hud->settings.value("antialias", 50), 0, 100);
    hud->refreshStyle();
    auto bounds = ReadJson(hud->boundsPath);
    if (bounds.is_object())
    {
        hud->x = bounds.value("x", 100);
        hud->y = bounds.value("y", 40);
        hud->width = std::clamp(bounds.value("width", 770), 420, 2000);
        hud->height = std::clamp(bounds.value("height", 223), 110, 1000);
    }
    RECT area{hud->x, hud->y, hud->x + hud->width, hud->y + hud->height};
    if (!MonitorFromRect(&area, MONITOR_DEFAULTTONULL))
    {
        hud->x = 100;
        hud->y = 40;
    }
    WNDCLASSW cls{};
    cls.lpfnWndProc = HudProc;
    cls.hInstance = module;
    cls.lpszClassName = L"SpotifyNativeLyricsHUD";
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&cls);
    hud->window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                  cls.lpszClassName, L"Spotify Lyrics", WS_POPUP, hud->x, hud->y, hud->width,
                                  hud->height, nullptr, nullptr, module, nullptr);
    if (!hud->window)
        return;
    hud->updateGlass();
    hud->server.hwnd = hud->window;
    hud->server.token = token;
    hud->server.port = config.value("port", 27946);
    hud->health();
    hud->server.start();
    if (hud->enabled)
        ShowWindow(hud->window, SW_SHOWNOACTIVATE);
    SetTimer(hud->window, 2, 1000, nullptr);
    MSG message{};
    for (;;)
    {
        // Bound message draining so playback/input traffic cannot starve animation.
        for (int count = 0; count < 64 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++count)
        {
            if (message.message == WM_QUIT)
                return;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (hud->frames.due())
            hud->frames.advance(hud->tick());
        hud->frames.wait();
    }
}
