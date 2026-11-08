// MIT. Screen coordinates are physical pixels (the observer is per-monitor DPI aware).
#pragma once
#include <windows.h>
#include <cwchar>
#include <initializer_list>

inline bool CoversMonitor(const RECT &window, const RECT &monitor)
{
    constexpr LONG tolerance = 2;
    return window.left <= monitor.left + tolerance && window.top <= monitor.top + tolerance &&
           window.right >= monitor.right - tolerance && window.bottom >= monitor.bottom - tolerance;
}

inline bool IsFullscreenWindow(HWND window, DWORD hudPid)
{
    if (!window || !IsWindowVisible(window) || IsIconic(window) || window == GetDesktopWindow() ||
        window == GetShellWindow())
        return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (!pid || pid == hudPid)
        return false;
    wchar_t name[64] = {};
    GetClassNameW(window, name, ARRAYSIZE(name));
    for (const auto *shell : {L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd"})
    {
        if (wcscmp(name, shell) == 0)
            return false;
    }
    // A normal maximized window with a title bar is still an interactive desktop app.
    if (IsZoomed(window) && (GetWindowLongPtrW(window, GWL_STYLE) & WS_CAPTION))
        return false;
    RECT bounds = {};
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    if (!GetWindowRect(window, &bounds) ||
        !GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
        return false;
    return CoversMonitor(bounds, monitor.rcMonitor);
}
