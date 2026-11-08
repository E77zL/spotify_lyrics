// MIT. Shared primitives for the in-process Spotify HUD. No helper executables.
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "vendor/json.hpp"
#include "NativeVersion.h"
using Json = nlohmann::json;
inline std::wstring Wide(const std::string &s)
{
    if (s.empty())
        return {};
    int n =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n)
        return {};
    std::wstring r(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), r.data(), n);
    return r;
}
inline std::string Utf8(const std::wstring &s)
{
    if (s.empty())
        return {};
    int n =
        WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string r(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), r.data(), n, nullptr, nullptr);
    return r;
}
inline Json ReadJson(const std::wstring &path)
{
    std::ifstream f{std::filesystem::path(path)};
    return f ? Json::parse(f, nullptr, false, true) : Json();
}
inline bool WriteJson(const std::wstring &path, const Json &value)
{
    const auto temp = path + L".native.tmp";
    try
    {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        {
            std::ofstream f{std::filesystem::path(temp), std::ios::binary | std::ios::trunc};
            auto bytes = value.dump(2);
            f.write(bytes.data(), bytes.size());
            f.flush();
            if (!f)
                return false;
        }
        return !!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }
    catch (...)
    {
        return false;
    }
}
inline std::vector<unsigned char> HashBytes(const wchar_t *algorithm, const std::string &bytes)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD size = 0, cb = 0;
    std::vector<unsigned char> result;
    if (BCryptOpenAlgorithmProvider(&alg, algorithm, nullptr, 0) < 0)
        return result;
    if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&size), sizeof(size), &cb, 0) >=
            0 &&
        BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0)
    {
        result.resize(size);
        if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char *>(bytes.data())),
                           static_cast<ULONG>(bytes.size()), 0) < 0 ||
            BCryptFinishHash(hash, result.data(), size, 0) < 0)
            result.clear();
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}
inline std::string HexHash(const wchar_t *algorithm, const std::string &data)
{
    std::string s;
    for (auto b : HashBytes(algorithm, data))
    {
        s += "0123456789abcdef"[b >> 4];
        s += "0123456789abcdef"[b & 15];
    }
    return s;
}
inline std::string Base64(const std::vector<unsigned char> &bytes)
{
    DWORD n = 0;
    CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()),
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &n);
    std::string s(n, 0);
    if (!CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, s.data(), &n))
        return {};
    s.resize(n);
    return s;
}
struct RequestJob
{
    std::atomic<HINTERNET> handle{nullptr};
    std::atomic_bool cancelled{false};
    ~RequestJob()
    {
        close();
    }
    void close()
    {
        auto h = handle.exchange(nullptr);
        if (h)
            WinHttpCloseHandle(h);
    }
    void cancel()
    {
        cancelled = true;
        close();
    }
    bool attach(HINTERNET h)
    {
        handle = h;
        if (cancelled)
        {
            close();
            return false;
        }
        return true;
    }
};
constexpr UINT HudMessage = WM_APP + 91;
struct Connection
{
    SOCKET socket = INVALID_SOCKET;
    std::mutex write, jobsMutex;
    std::map<int, std::shared_ptr<RequestJob>> jobs;
    void cancel(int id)
    {
        std::lock_guard<std::mutex> lock(jobsMutex);
        auto it = jobs.find(id);
        if (it != jobs.end())
            it->second->cancel();
    }
    void cancelAll()
    {
        std::lock_guard<std::mutex> lock(jobsMutex);
        for (auto &item : jobs)
            item.second->cancel();
    }
    std::atomic_bool open{true};
    ~Connection()
    {
        if (socket != INVALID_SOCKET)
            closesocket(socket);
    }
    bool sendRaw(const std::string &bytes)
    {
        std::lock_guard<std::mutex> lock(write);
        if (!open)
            return false;
        size_t offset = 0;
        while (offset < bytes.size())
        {
            int n = send(socket, bytes.data() + offset, static_cast<int>(bytes.size() - offset), 0);
            if (n <= 0)
            {
                open = false;
                return false;
            }
            offset += n;
        }
        return true;
    }
    void sendJson(const Json &j)
    {
        auto bytes = j.dump();
        std::string frame;
        frame += static_cast<char>(0x81);
        if (bytes.size() < 126)
            frame += static_cast<char>(bytes.size());
        else if (bytes.size() < 65536)
        {
            frame += static_cast<char>(126);
            frame += static_cast<char>(bytes.size() >> 8);
            frame += static_cast<char>(bytes.size());
        }
        else
        {
            frame += static_cast<char>(127);
            for (int i = 7; i >= 0; --i)
                frame += static_cast<char>(static_cast<uint64_t>(bytes.size()) >> (8 * i));
        }
        sendRaw(frame + bytes);
    }
};
struct NativeServer
{
    HWND hwnd = nullptr;
    std::string token;
    int port = 27946;
    std::mutex clientsMutex, healthMutex;
    std::vector<std::shared_ptr<Connection>> clients;
    std::string health = "{}";
    std::atomic_int workers{0}, connections{0};
    bool post(Json message)
    {
        auto data = std::make_unique<Json>(std::move(message));
        if (!PostMessageW(hwnd, HudMessage, 0, reinterpret_cast<LPARAM>(data.get())))
            return false;
        data.release(); // The HUD window now owns the message.
        return true;
    }
    void broadcast(const Json &message)
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(), [](auto c) { return !c->open; }),
                      clients.end());
        for (auto c : clients)
            c->sendJson(message);
    }
    void start();
    void handle(const std::shared_ptr<Connection> &c);
    void request(const std::shared_ptr<Connection> &c, const Json &message);
};
