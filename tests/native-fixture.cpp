// MIT. A real Windows import-table host for same-process native HUD QA.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winver.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <psapi.h>
#include <dwmapi.h>

static std::wstring Env(const wchar_t *key)
{
    wchar_t value[32768] = {};
    GetEnvironmentVariableW(key, value, ARRAYSIZE(value));
    return value;
}

static void Report(const wchar_t *name, const std::string &content)
{
    FILE *file = nullptr;
    const auto path = Env(L"HUD_TEST_DIR") + L"\\" + name;
    if (_wfopen_s(&file, path.c_str(), L"wb") == 0)
    {
        fwrite(content.data(), 1, content.size(), file);
        fclose(file);
    }
}

int wmain()
{
    wchar_t system[MAX_PATH] = {};
    GetSystemDirectoryW(system, MAX_PATH);
    const std::wstring file = std::wstring(system) + L"\\kernel32.dll";
    const auto real = LoadLibraryW((std::wstring(system) + L"\\version.dll").c_str());
    if (!real)
        return 20;
    const auto sizeFn =
        reinterpret_cast<decltype(&GetFileVersionInfoSizeW)>(GetProcAddress(real, "GetFileVersionInfoSizeW"));
    const auto readFn =
        reinterpret_cast<decltype(&GetFileVersionInfoW)>(GetProcAddress(real, "GetFileVersionInfoW"));
    DWORD ignored;
    const auto size = GetFileVersionInfoSizeW(file.c_str(), &ignored);
    if (!size || !sizeFn || !readFn || size != sizeFn(file.c_str(), &ignored))
        return 21;
    std::vector<unsigned char> proxied(size), original(size);
    if (!GetFileVersionInfoW(file.c_str(), 0, size, proxied.data()) ||
        !readFn(file.c_str(), 0, size, original.data()) || proxied != original)
        return 22;
    LPVOID value = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(proxied.data(), L"\\", &value, &length) || length < sizeof(VS_FIXEDFILEINFO) ||
        static_cast<VS_FIXEDFILEINFO *>(value)->dwSignature != VS_FFI_SIGNATURE)
        return 23;
    wchar_t module[32768] = {};
    GetModuleFileNameW(GetModuleHandleW(L"version.dll"), module, ARRAYSIZE(module));
    wchar_t exe[32768] = {};
    GetModuleFileNameW(nullptr, exe, ARRAYSIZE(exe));
    std::wstring directory(exe);
    directory.resize(directory.find_last_of(L'\\'));
    if (_wcsicmp(module, (directory + L"\\version.dll").c_str()) != 0)
        return 24;
    FreeLibrary(real);
    Report(L"host.json", "{\"forwarding\":true,\"pid\":" + std::to_string(GetCurrentProcessId()) + "}");
    std::string command;
    POINT pointer{};
    RECT layoutBounds{};
    RECT originalBounds{};
    while (std::getline(std::cin, command))
    {
        HWND window = nullptr;
        DWORD pid = 0;
        while ((window = FindWindowExW(nullptr, window, L"SpotifyNativeLyricsHUD", nullptr)))
        {
            GetWindowThreadProcessId(window, &pid);
            if (pid == GetCurrentProcessId())
                break;
        }
        if (!window)
            return 25;
        RECT rect{};
        GetWindowRect(window, &rect);
        if (!originalBounds.right)
            originalBounds = rect;
        const auto start = std::chrono::steady_clock::now();
        if (command == "frame")
        {
            DWM_WINDOW_CORNER_PREFERENCE corner{};
            const auto cornerResult = DwmGetWindowAttribute(
                window, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
            std::cout << ((!SUCCEEDED(cornerResult) || corner != DWMWCP_DONOTROUND) ? "system" : "custom")
                      << std::endl;
            continue;
        }
        if (command == "hitBlank")
        {
            const POINT blank{rect.left + 20, rect.top + 45};
            std::cout << (WindowFromPoint(blank) == window ? "hit" : "pass-through") << std::endl;
            continue;
        }
        if (command == "resources")
        {
            FILETIME creation{}, exit{}, kernel{}, user{};
            GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user);
            const auto time = [](FILETIME t) {
                return (static_cast<unsigned long long>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
            };
            PROCESS_MEMORY_COUNTERS_EX memory{};
            GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),
                                 sizeof(memory));
            std::cout << "{\"cpuMs\":" << (time(kernel) + time(user)) / 10000.0
                      << ",\"privateBytes\":" << memory.PrivateUsage << "}" << std::endl;
            continue;
        }
        if (command == "layoutNarrow" || command == "layoutWide" || command == "layoutShort" ||
            command == "layoutRestore")
        {
            if (!layoutBounds.right)
                layoutBounds = rect;
            const bool restore = command == "layoutRestore";
            SetWindowPos(window, nullptr, layoutBounds.left, layoutBounds.top,
                         restore                   ? layoutBounds.right - layoutBounds.left
                         : command == "layoutWide" ? 1400
                                                   : 420,
                         restore                    ? layoutBounds.bottom - layoutBounds.top
                         : command == "layoutShort" ? 110
                                                    : 600,
                         SWP_NOACTIVATE | SWP_NOZORDER);
        }
        else if (command == "layoutRestoreNormal")
            SetWindowPos(window, nullptr, originalBounds.left, originalBounds.top,
                         originalBounds.right - originalBounds.left, originalBounds.bottom - originalBounds.top,
                         SWP_NOACTIVATE | SWP_NOZORDER);
        else if (command == "hover")
            SendMessageW(window, WM_MOUSEMOVE, 0, MAKELPARAM(20, 45));
        else if (command == "topBegin")
        {
            pointer = {rect.left + 200, rect.top + 4};
            SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(200, 4));
        }
        else if (command.rfind("key", 0) == 0)
        {
            // The fixture's caller sends real messages to the HUD thread, without global hooks.
            const WPARAM key = command == "keyLeft" ? VK_LEFT : command == "keyRight" ? VK_RIGHT :
                               command == "keyUp" ? VK_UP : command == "keyDown" ? VK_DOWN : VK_SPACE;
            SendMessageW(window, WM_KEYDOWN, key, command == "keySpaceRepeat" ? (1LL << 30) : 0);
        }
        else if (command == "begin" || command == "resizeBegin")
        {
            const int px = command == "begin" ? 300 : rect.right - rect.left - 10;
            const int py = command == "begin" ? 130 : rect.bottom - rect.top - 10;
            pointer = {rect.left + px, rect.top + py};
            SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(px, py));
        }
        else if (command == "end")
            SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(pointer.x - rect.left, pointer.y - rect.top));
        else if (command == "cancel")
            SendMessageW(window, WM_KEYDOWN, VK_ESCAPE, 0);
        else if (command == "step" || command == "maxStep" || command == "minStep")
        {
            const int delta = command == "step" ? 80 : command == "maxStep" ? 5000 : -10000;
            pointer.x += delta;
            pointer.y += delta / 2;
            SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON,
                         MAKELPARAM(pointer.x - rect.left, pointer.y - rect.top));
        }
        else if (command == "moveBurst" || command == "resizeBurst")
        {
            for (int i = 0; i < 512; ++i)
            {
                GetWindowRect(window, &rect);
                const int delta = i % 128;
                SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON,
                             MAKELPARAM(pointer.x + delta - rect.left, pointer.y + delta / 2 - rect.top));
            }
            GetWindowRect(window, &rect);
            SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON,
                         MAKELPARAM(pointer.x - rect.left, pointer.y - rect.top));
        }
        else
        {
            const int item = command == "fullscreen" ? 4 : command == "typography" ? 1 : 5;
            SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON,
                         MAKELPARAM(rect.right - rect.left - 234 + 38 * item + 16, 16));
        }
        const auto elapsed =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
        std::cout << "ok " << elapsed.count() << std::endl;
    }
    return 0;
}
