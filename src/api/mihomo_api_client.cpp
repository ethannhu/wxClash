#include "mihomo_api_client.h"

#include <nlohmann/json.hpp>

#include <wx/socket.h>

#include <cstdint>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>
#include <utility>

namespace
{
    constexpr int kStreamingSnapshotTimeoutMs = 2500;

    std::optional<std::string> HeaderValue(const std::string& raw,
                                           const char* headerName)
    {
        const auto headerEnd = raw.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
            return std::nullopt;

        const std::string expected(headerName);
        std::size_t lineStart = 0;
        while (lineStart < headerEnd)
        {
            const auto lineEnd = raw.find("\r\n", lineStart);
            if (lineEnd == std::string::npos || lineEnd > headerEnd)
                break;
            const auto colon = raw.find(':', lineStart);
            if (colon != std::string::npos && colon < lineEnd)
            {
                auto name = raw.substr(lineStart, colon - lineStart);
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char character) {
                                   return static_cast<char>(std::tolower(character));
                               });
                if (name == expected)
                {
                    auto valueStart = raw.find_first_not_of(" \t", colon + 1);
                    if (valueStart == std::string::npos || valueStart >= lineEnd)
                        return std::string{};
                    return raw.substr(valueStart, lineEnd - valueStart);
                }
            }
            lineStart = lineEnd + 2;
        }
        return std::nullopt;
    }

    std::optional<std::size_t> ContentLength(const std::string& raw)
    {
        const auto value = HeaderValue(raw, "content-length");
        if (!value)
            return std::nullopt;
        try
        {
            return std::stoull(*value);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::optional<std::string> DecodeChunkedBody(const std::string& raw,
                                                 bool firstChunkOnly)
    {
        const auto headerEnd = raw.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
            return std::nullopt;

        const auto transferEncoding = HeaderValue(raw, "transfer-encoding");
        if (!transferEncoding)
            return std::nullopt;
        std::string encoding = *transferEncoding;
        std::transform(encoding.begin(), encoding.end(), encoding.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(std::tolower(character));
                       });
        if (encoding.find("chunked") == std::string::npos)
            return std::nullopt;

        std::string body;
        std::size_t offset = headerEnd + 4;
        while (true)
        {
            const auto sizeEnd = raw.find("\r\n", offset);
            if (sizeEnd == std::string::npos)
                return std::nullopt;
            const auto extension = raw.find(';', offset);
            const auto sizeTextEnd = extension != std::string::npos && extension < sizeEnd
                                         ? extension
                                         : sizeEnd;

            std::size_t chunkSize = 0;
            try
            {
                chunkSize = std::stoull(
                    raw.substr(offset, sizeTextEnd - offset), nullptr, 16);
            }
            catch (...)
            {
                return std::nullopt;
            }

            const auto chunkStart = sizeEnd + 2;
            if (raw.size() < chunkStart + chunkSize + 2)
                return std::nullopt;
            if (raw.compare(chunkStart + chunkSize, 2, "\r\n") != 0)
                return std::nullopt;

            if (chunkSize == 0)
                return body;

            body.append(raw, chunkStart, chunkSize);
            if (firstChunkOnly)
                return body;
            offset = chunkStart + chunkSize + 2;
        }
    }

    bool IsStreamingSnapshot(const std::string& method, const std::string& path)
    {
        if (method != "GET")
            return false;
        return path == "/traffic" || path == "/memory" ||
               path == "/connections";
    }

    bool ParseUrl(const std::string& url, std::string& host, int& port)
    {
        constexpr const char* prefix = "http://";
        if (url.rfind(prefix, 0) != 0)
            return false;
        auto value = url.substr(7);
        const auto slash = value.find('/');
        value = value.substr(0, slash);
        const auto colon = value.rfind(':');
        if (colon == std::string::npos)
        {
            host = value;
            port = 80;
            return !host.empty();
        }
        host = value.substr(0, colon);
        try
        {
            port = std::stoi(value.substr(colon + 1));
        }
        catch (...)
        {
            return false;
        }
        return !host.empty() && port > 0 && port < 65536;
    }

    std::string UrlEncode(const std::string& value)
    {
        static constexpr char hex[] = "0123456789ABCDEF";
        std::string encoded;
        for (const unsigned char character : value)
        {
            if ((character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') ||
                character == '-' || character == '_' || character == '.' ||
                character == '~')
                encoded.push_back(static_cast<char>(character));
            else
            {
                encoded.push_back('%');
                encoded.push_back(hex[character >> 4]);
                encoded.push_back(hex[character & 0x0F]);
            }
        }
        return encoded;
    }
}

MihomoApiClient::MihomoApiClient(MihomoApiConfig config)
    : config_(std::move(config))
{
}

MihomoApiResponse MihomoApiClient::Request(const std::string& method,
                                           const std::string& path,
                                           const std::string& body) const
{
    MihomoApiResponse response;
    std::string host;
    int port = 0;
    if (!ParseUrl(config_.baseUrl, host, port))
    {
        response.error = "Only http:// Controller URLs are supported";
        return response;
    }

    wxIPV4address address;
    address.Hostname(host);
    address.Service(port);
    wxSocketClient socket;
    const bool streamingSnapshot = IsStreamingSnapshot(method, path);
    const int timeoutMs = streamingSnapshot
                              ? std::max(config_.timeoutMs, kStreamingSnapshotTimeoutMs)
                              : config_.timeoutMs;
    socket.SetTimeout(static_cast<unsigned>(std::max(1, (timeoutMs + 999) / 1000)));
    if (!socket.Connect(address, true))
    {
        response.error = "Unable to connect to Mihomo Controller";
        return response;
    }

    const std::string request = method + " " + path + " HTTP/1.1\r\n" +
                                "Host: " + host + "\r\n" +
                                "Authorization: Bearer " + config_.secret + "\r\n" +
                                "Content-Type: application/json\r\n" +
                                "Connection: close\r\n" +
                                "Content-Length: " + std::to_string(body.size()) +
                                "\r\n\r\n" + body;
    std::size_t written = 0;
    while (written < request.size())
    {
        if (!socket.WaitForWrite(0, 100))
        {
            response.error = "Timed out sending Controller request";
            return response;
        }
        const auto remaining = request.size() - written;
        const auto writeSize = static_cast<wxUint32>(std::min<std::size_t>(
            remaining, std::numeric_limits<wxUint32>::max()));
        socket.Write(request.data() + written, writeSize);
        const auto count = socket.LastWriteCount();
        if (socket.Error() || count == 0)
        {
            response.error = "Unable to send Controller request";
            return response;
        }
        written += count;
    }

    std::string raw;
    char buffer[4096];
    std::optional<std::string> chunkedBody;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        const auto waitMs = static_cast<long>(std::min<std::int64_t>(100, remaining));
        if (waitMs <= 0 || !socket.WaitForRead(0, waitMs))
            continue;
        socket.Read(buffer, sizeof(buffer));
        const auto count = socket.LastReadCount();
        if (count == 0)
            break;
        raw.append(buffer, count);

        // wxSocket may retain an EOF/error flag together with the final bytes.
        // The HTTP response is still usable when those bytes complete it.
        if (socket.Error() && raw.find("\r\n\r\n") == std::string::npos)
        {
            response.error = "Unable to read Controller response";
            return response;
        }

        const auto headerEnd = raw.find("\r\n\r\n");
        if (headerEnd != std::string::npos)
        {
            chunkedBody = DecodeChunkedBody(raw, streamingSnapshot);
            if (chunkedBody)
                break;
        }
        const auto length = ContentLength(raw);
        if (headerEnd != std::string::npos && length &&
            raw.size() - headerEnd - 4 >= *length)
            break;
    }

    const auto headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        const bool timedOut = std::chrono::steady_clock::now() >= deadline;
        response.error = raw.empty()
                             ? (timedOut
                                    ? "Controller did not respond before timeout"
                                    : "Controller closed connection without a response")
                             : "Controller response did not contain an HTTP header";
        return response;
    }
    const auto firstLineEnd = raw.find("\r\n");
    if (firstLineEnd == std::string::npos)
    {
        response.error = "Invalid Controller status line";
        return response;
    }
    const auto statusStart = raw.find(' ', 0);
    if (statusStart == std::string::npos)
    {
        response.error = "Invalid Controller status line";
        return response;
    }
    try
    {
        response.status = std::stoi(raw.substr(statusStart + 1, 3));
    }
    catch (...)
    {
        response.error = "Invalid Controller status code";
        return response;
    }
    response.body = raw.substr(headerEnd + 4);
    if (HeaderValue(raw, "transfer-encoding"))
    {
        if (!chunkedBody)
        {
            response.error = "Incomplete chunked Controller response";
            return response;
        }
        response.body = *chunkedBody;
    }
    response.ok = response.status >= 200 && response.status < 300;
    if (!response.ok)
    {
        response.error = "Controller returned HTTP " + std::to_string(response.status);
        if (!response.body.empty())
            response.error += ": " + response.body;
    }
    return response;
}

MihomoApiResponse MihomoApiClient::SelectProxy(const std::string& group,
                                               const std::string& proxy) const
{
    return Request("PUT", "/proxies/" + UrlEncode(group),
                   nlohmann::json{{"name", proxy}}.dump());
}

MihomoApiResponse MihomoApiClient::GetProxy(const std::string& group) const
{
    return Request("GET", "/proxies/" + UrlEncode(group));
}
