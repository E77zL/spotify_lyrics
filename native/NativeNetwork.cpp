// MIT. Loopback transport and bounded provider requests, hosted by Spotify.exe.
#include "NativeCore.h"
#include <string_view>
struct Internet
{
    HINTERNET h = nullptr;
    explicit Internet(HINTERNET value) : h(value) {}
    ~Internet()
    {
        if (h)
            WinHttpCloseHandle(h);
    }
};
static Json ProviderRequest(const Json &args, const std::shared_ptr<RequestJob> &job)
{
    auto url = Wide(args.at("url").get<std::string>());
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
        static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS ||
        parts.nPort != 443 || parts.dwUserNameLength || parts.dwPasswordLength)
        throw std::runtime_error("Invalid provider URL");
    std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    static constexpr std::wstring_view hosts[] = {L"lrclib.net",
                                             L"c.y.qq.com",
                                             L"u.y.qq.com",
                                             L"interface.music.163.com",
                                             L"interface3.music.163.com",
                                             L"music.163.com"};
    if (std::find(std::begin(hosts), std::end(hosts), host) == std::end(hosts))
        throw std::runtime_error("Provider not allowed");
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength)
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    const auto method = args.value("method", std::string("GET"));
    if (method != "GET" && method != "POST")
        throw std::runtime_error("Invalid method");
    auto body = args.value("body", std::string());
    if (body.size() > 65536)
        throw std::runtime_error("Request too large");
    const auto agent = L"SpotifyLyricsHUD/" + Wide(HudVersion);
    Internet session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0));
    const int timeout = std::clamp(args.value("timeout", 8000), 500, 8000);
    if (!session.h)
        throw std::runtime_error("Network unavailable");
    WinHttpSetTimeouts(session.h, timeout, timeout, timeout, timeout);
    Internet connection(WinHttpConnect(session.h, host.c_str(), 443, 0));
    Internet request(connection.h ? WinHttpOpenRequest(connection.h, Wide(method).c_str(), path.c_str(),
                                                       nullptr, WINHTTP_NO_REFERER,
                                                       WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                  : nullptr);
    if (!request.h)
        throw std::runtime_error("Network request failed");
    auto requestHandle = request.h;
    request.h = nullptr;
    if (!job->attach(requestHandle))
        throw std::runtime_error("Cancelled");
    struct CloseRequest
    {
        std::shared_ptr<RequestJob> job;
        ~CloseRequest()
        {
            job->close();
        }
    } closeRequest{job};
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(requestHandle, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
    std::wstring headers;
    if (args.contains("headers") && args["headers"].is_object())
        for (auto it = args["headers"].begin(); it != args["headers"].end(); ++it)
        {
            const auto key = it.key();
            if (key != "Referer" && key != "User-Agent" && key != "Content-Type")
                continue;
            auto value = it.value().get<std::string>();
            if (value.find_first_of("\r\n") != std::string::npos)
                continue;
            headers += Wide(key + ": " + value + "\r\n");
        }
    if (!WinHttpSendRequest(requestHandle, headers.c_str(), static_cast<DWORD>(headers.size()),
                            body.empty() ? WINHTTP_NO_REQUEST_DATA : body.data(),
                            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) ||
        !WinHttpReceiveResponse(requestHandle, nullptr))
        throw std::runtime_error("Provider connection failed");
    DWORD code = 0, cb = sizeof(code);
    WinHttpQueryHeaders(requestHandle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &code, &cb, WINHTTP_NO_HEADER_INDEX);
    if (code < 200 || code >= 300)
        throw std::runtime_error("Provider HTTP " + std::to_string(code));
    std::string data;
    char buffer[16384];
    DWORD n = 0;
    auto started = GetTickCount64();
    do
    {
        if (!WinHttpReadData(requestHandle, buffer, sizeof(buffer), &n))
            throw std::runtime_error("Provider read failed");
        data.append(buffer, n);
        if (data.size() > 4 * 1024 * 1024 || GetTickCount64() - started > static_cast<ULONGLONG>(timeout))
            throw std::runtime_error("Provider response limit");
    } while (n);
    job->close();
    return Json::parse(data);
}
void NativeServer::request(const std::shared_ptr<Connection> &c, const Json &message)
{
    if (workers.fetch_add(1) >= 4)
    {
        workers--;
        c->sendJson({{"type", "reply"}, {"id", message.value("id", 0)}, {"error", "Provider busy"}});
        return;
    }
    const int id = message.value("id", 0);
    auto job = std::make_shared<RequestJob>();
    {
        std::lock_guard<std::mutex> lock(c->jobsMutex);
        if (c->jobs.count(id))
        {
            workers--;
            return;
        }
        c->jobs[id] = job;
    }
    std::thread([this, c, message, job, id] {
        Json reply = {{"type", "reply"}, {"id", message.value("id", 0)}};
        try
        {
            const auto op = message.value("op", std::string());
            if (op == "fetch")
                reply["result"] = ProviderRequest(message.at("args"), job);
            else if (op == "md5")
            {
                auto value = message.at("args").get<std::string>();
                if (value.size() > 65536)
                    throw std::runtime_error("Input too large");
                reply["result"] = HexHash(BCRYPT_MD5_ALGORITHM, value);
            }
            else
                throw std::runtime_error("Unknown request");
        }
        catch (const std::exception &e)
        {
            reply["error"] = e.what();
        }
        catch (...)
        {
            reply["error"] = "Provider failure";
        }
        job->close();
        {
            std::lock_guard<std::mutex> lock(c->jobsMutex);
            c->jobs.erase(id);
        }
        if (!job->cancelled)
            c->sendJson(reply);
        workers--;
    }).detach();
}
static bool ReadExact(SOCKET socket, char *out, size_t count)
{
    while (count)
    {
        int n = recv(socket, out, static_cast<int>(count), 0);
        if (n <= 0)
            return false;
        out += n;
        count -= n;
    }
    return true;
}
void NativeServer::handle(const std::shared_ptr<Connection> &c)
{
    try
    {
        std::string header;
        char ch;
        while (header.size() < 16384 && ReadExact(c->socket, &ch, 1))
        {
            header += ch;
            if (header.size() >= 4 && header.compare(header.size() - 4, 4, "\r\n\r\n") == 0)
                break;
        }
        auto end = header.find("\r\n");
        if (end == std::string::npos)
            throw std::runtime_error("Bad HTTP");
        auto first = header.substr(0, end);
        std::map<std::string, std::string> fields;
        for (size_t pos = end + 2; pos < header.size();)
        {
            end = header.find("\r\n", pos);
            if (end == std::string::npos || end == pos)
                break;
            auto row = header.substr(pos, end - pos);
            auto sep = row.find(':');
            if (sep == std::string::npos)
                throw std::runtime_error("Bad header");
            auto key = row.substr(0, sep);
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char b) { return static_cast<char>(tolower(b)); });
            auto value = row.substr(sep + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            fields[key] = value;
            pos = end + 2;
        }
        if (first == "GET /health HTTP/1.1" && fields["x-hud-token"] == token)
        {
            std::string data;
            {
                std::lock_guard<std::mutex> lock(healthMutex);
                data = health;
            }
            c->sendRaw(
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                std::to_string(data.size()) + "\r\n\r\n" + data);
        }
        else if (first == "GET /control?token=" + token + " HTTP/1.1" && fields["upgrade"] == "websocket" &&
                 fields["sec-websocket-version"] == "13" && fields["sec-websocket-key"].size() == 24 &&
                 (fields["origin"] == "https://xpui.app.spotify.com" ||
                  fields["origin"] == "https://open.spotify.com" ||
                  fields["origin"] == "https://zlink.app.spotify.com"))
        {
            const auto accept = Base64(HashBytes(
                BCRYPT_SHA1_ALGORITHM, fields["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
            if (!c->sendRaw("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: "
                            "Upgrade\r\nSec-WebSocket-Accept: " +
                            accept + "\r\n\r\n"))
                throw std::runtime_error("Closed");
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients.push_back(c);
            }
            if (!post({{"type", "connected"}}))
                throw std::runtime_error("HUD window unavailable");
            while (c->open)
            {
                unsigned char base[2];
                if (!ReadExact(c->socket, reinterpret_cast<char *>(base), 2))
                    break;
                const int opcode = base[0] & 15;
                if (!(base[0] & 128) || (base[0] & 112) || !(base[1] & 128))
                    break;
                uint64_t n = base[1] & 127;
                if (n == 126)
                {
                    unsigned char b[2];
                    if (!ReadExact(c->socket, reinterpret_cast<char *>(b), 2))
                        break;
                    n = (b[0] << 8) | b[1];
                }
                else if (n == 127)
                {
                    unsigned char b[8];
                    if (!ReadExact(c->socket, reinterpret_cast<char *>(b), 8))
                        break;
                    n = 0;
                    for (auto byte : b)
                        n = (n << 8) | byte;
                }
                if (n > 2 * 1024 * 1024 || (opcode >= 8 && n > 125))
                    break;
                unsigned char mask[4];
                if (!ReadExact(c->socket, reinterpret_cast<char *>(mask), 4))
                    break;
                std::string payload(static_cast<size_t>(n), 0);
                if (!ReadExact(c->socket, payload.data(), payload.size()))
                    break;
                for (size_t i = 0; i < payload.size(); ++i)
                    payload[i] ^= mask[i % 4];
                if (opcode == 8)
                    break;
                if (opcode == 9)
                {
                    std::string pong;
                    pong += static_cast<char>(0x8a);
                    pong += static_cast<char>(n);
                    c->sendRaw(pong + payload);
                    continue;
                }
                if (opcode != 1)
                    break;
                auto message = Json::parse(payload, nullptr, false);
                if (!message.is_object())
                    break;
                if (message.value("type", std::string()) == "cancel")
                    c->cancel(message.value("id", 0));
                else if (message.value("type", std::string()) == "request")
                    request(c, message);
                else if (!post(std::move(message)))
                    break;
            }
        }
        else
            c->sendRaw("HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    }
    catch (...)
    {
    }
    c->open = false;
    c->cancelAll();
    shutdown(c->socket, SD_BOTH);
    connections--;
}
void NativeServer::start()
{
    std::thread([this] {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data))
            return;
        SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET)
            return;
        BOOL exclusive = TRUE;
        setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char *>(&exclusive),
                   sizeof(exclusive));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<u_short>(port));
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) || listen(listener, 4))
        {
            closesocket(listener);
            return;
        }
        for (;;)
        {
            auto client = accept(listener, nullptr, nullptr);
            if (client == INVALID_SOCKET)
                break;
            if (connections.fetch_add(1) >= 4)
            {
                connections--;
                closesocket(client);
                continue;
            }
            DWORD timeout = 15000;
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));
            setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));
            auto c = std::make_shared<Connection>();
            c->socket = client;
            std::thread([this, c] { handle(c); }).detach();
        }
        closesocket(listener);
    }).detach();
}
