#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include "../native/fullscreen.h"

int main() {
    const RECT main = { 0, 0, 1920, 1080 }, left = { -2560, -360, 0, 1080 };
    assert(CoversMonitor(main, main));
    assert(CoversMonitor({ -1, -1, 1921, 1081 }, main));
    assert(CoversMonitor({ 2, 2, 1918, 1078 }, main));
    assert(!CoversMonitor({ 3, 0, 1920, 1080 }, main));
    assert(!CoversMonitor({ 0, 0, 1920, 1040 }, main)); // Maximized work area.
    assert(!CoversMonitor({ 10, 10, 1910, 1070 }, main));
    assert(CoversMonitor(left, left)); // Mixed DPI / negative monitor coordinates.
    assert(!CoversMonitor(main, left));
    assert(CoversMonitor({ -2560, -360, 1920, 1080 }, left)); // Spanning window.
    assert(!IsFullscreenWindow(nullptr, GetCurrentProcessId()));
    assert(!IsFullscreenWindow(GetDesktopWindow(), GetCurrentProcessId()));
    assert(!IsFullscreenWindow(GetShellWindow(), GetCurrentProcessId()));
    puts("PASS native screen coverage, taskbar/work-area distinction, tolerance, multiple monitors, spanning windows and shell exclusions");
}
