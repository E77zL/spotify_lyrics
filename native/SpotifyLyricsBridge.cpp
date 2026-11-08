// MIT. Loaded by the user's Spotify installation; no remote-process injection.
// The Windows version APIs remain backed by the real System32 version.dll.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winver.h>
#include <bcrypt.h>
#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>
#include "NativeHud.h"

static HMODULE moduleHandle;
static INIT_ONCE versionOnce = INIT_ONCE_STATIC_INIT;
static HMODULE systemVersion;

static BOOL CALLBACK LoadSystemVersion(PINIT_ONCE, PVOID, PVOID *)
{
    wchar_t file[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(file, MAX_PATH);
    if (length && length < MAX_PATH - 13 && wcscat_s(file, L"\\version.dll") == 0)
        systemVersion = LoadLibraryExW(file, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return TRUE; // A failed forward reports an API error; never stop Spotify.
}

static FARPROC VersionApi(const char *name)
{
    InitOnceExecuteOnce(&versionOnce, LoadSystemVersion, nullptr, nullptr);
    const FARPROC result = systemVersion ? GetProcAddress(systemVersion, name) : nullptr;
    if (!result)
        SetLastError(ERROR_PROC_NOT_FOUND);
    return result;
}

// The prefix avoids redefining the SDK's dllimport declarations. The .def maps
// public Windows names to these strictly typed wrappers.
#define FORWARD(name, type, params, args)                                                                    \
    extern "C" type WINAPI Hud_##name params                                                                 \
    {                                                                                                        \
        static const auto function = reinterpret_cast<decltype(&::name)>(VersionApi(#name));                 \
        if (!function)                                                                                       \
        {                                                                                                    \
            SetLastError(ERROR_PROC_NOT_FOUND);                                                              \
            return 0;                                                                                        \
        }                                                                                                    \
        return function args;                                                                                \
    }
FORWARD(GetFileVersionInfoSizeA, DWORD, (LPCSTR f, LPDWORD h), (f, h))
FORWARD(GetFileVersionInfoSizeW, DWORD, (LPCWSTR f, LPDWORD h), (f, h))
FORWARD(GetFileVersionInfoSizeExA, DWORD, (DWORD flags, LPCSTR f, LPDWORD h), (flags, f, h))
FORWARD(GetFileVersionInfoSizeExW, DWORD, (DWORD flags, LPCWSTR f, LPDWORD h), (flags, f, h))
FORWARD(GetFileVersionInfoA, BOOL, (LPCSTR f, DWORD h, DWORD n, LPVOID p), (f, h, n, p))
FORWARD(GetFileVersionInfoW, BOOL, (LPCWSTR f, DWORD h, DWORD n, LPVOID p), (f, h, n, p))
FORWARD(GetFileVersionInfoExA, BOOL, (DWORD flags, LPCSTR f, DWORD h, DWORD n, LPVOID p), (flags, f, h, n, p))
FORWARD(GetFileVersionInfoExW, BOOL, (DWORD flags, LPCWSTR f, DWORD h, DWORD n, LPVOID p),
        (flags, f, h, n, p))
FORWARD(VerQueryValueA, BOOL, (LPCVOID p, LPCSTR block, LPVOID *out, PUINT n), (p, block, out, n))
FORWARD(VerQueryValueW, BOOL, (LPCVOID p, LPCWSTR block, LPVOID *out, PUINT n), (p, block, out, n))
FORWARD(VerLanguageNameA, DWORD, (DWORD language, LPSTR out, DWORD n), (language, out, n))
FORWARD(VerLanguageNameW, DWORD, (DWORD language, LPWSTR out, DWORD n), (language, out, n))
FORWARD(VerFindFileA, DWORD,
        (DWORD flags, LPCSTR f, LPCSTR win, LPCSTR app, LPSTR current, PUINT cn, LPSTR dest, PUINT dn),
        (flags, f, win, app, current, cn, dest, dn))
FORWARD(VerFindFileW, DWORD,
        (DWORD flags, LPCWSTR f, LPCWSTR win, LPCWSTR app, LPWSTR current, PUINT cn, LPWSTR dest, PUINT dn),
        (flags, f, win, app, current, cn, dest, dn))
FORWARD(VerInstallFileA, DWORD,
        (DWORD flags, LPCSTR src, LPCSTR dest, LPCSTR sd, LPCSTR dd, LPCSTR current, LPSTR temp, PUINT n),
        (flags, src, dest, sd, dd, current, temp, n))
FORWARD(VerInstallFileW, DWORD,
        (DWORD flags, LPCWSTR src, LPCWSTR dest, LPCWSTR sd, LPCWSTR dd, LPCWSTR current, LPWSTR temp,
         PUINT n),
        (flags, src, dest, sd, dd, current, temp, n))

class Handle
{
  public:
    HANDLE value;
    explicit Handle(HANDLE handle = nullptr) : value(handle) {}
    ~Handle()
    {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    explicit operator bool() const
    {
        return value && value != INVALID_HANDLE_VALUE;
    }
};

static std::wstring ModuleDirectory()
{
    wchar_t file[32768] = {};
    const DWORD length = GetModuleFileNameW(moduleHandle, file, ARRAYSIZE(file));
    if (!length || length == ARRAYSIZE(file))
        return {};
    const wchar_t *slash = wcsrchr(file, L'\\');
    return slash ? std::wstring(file, static_cast<size_t>(slash - file)) : std::wstring();
}

static std::wstring Setting(const std::wstring &file, const wchar_t *key)
{
    wchar_t value[32768] = {};
    GetPrivateProfileStringW(L"LyricsHUD", key, L"", value, ARRAYSIZE(value), file.c_str());
    return value;
}

static DWORD WINAPI BridgeThread(void *)
{
    try
    {
        const auto spotify = ModuleDirectory();
        const auto config = spotify + L"\\LyricsHUD\\bridge.ini";
        const auto root = Setting(config, L"InstallRoot");
        if (!root.empty() && GetPrivateProfileIntW(L"LyricsHUD", L"Enabled", 0, config.c_str()))
            RunNativeHud(moduleHandle, spotify, root);
    }
    catch (...)
    { /* Never propagate a lyrics error into Spotify. */
    }
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        moduleHandle = instance;
        // Do not load another DLL or wait under the loader lock. The worker
        // starts only after Windows finishes this module's attach callback.
        wchar_t host[32768] = {};
        const DWORD length = GetModuleFileNameW(nullptr, host, ARRAYSIZE(host));
        const wchar_t *name = length && length < ARRAYSIZE(host) ? wcsrchr(host, L'\\') : nullptr;
#ifdef HUD_BRIDGE_TEST
        const wchar_t *expected = L"SpotifyBridgeTest.exe";
#else
        const wchar_t *expected = L"Spotify.exe";
#endif
        if (name && _wcsicmp(name + 1, expected) == 0 && !wcsstr(GetCommandLineW(), L"--type="))
        {
            HANDLE thread = CreateThread(nullptr, 0, BridgeThread, nullptr, 0, nullptr);
            if (thread)
                CloseHandle(thread);
        }
    }
    return TRUE;
}
