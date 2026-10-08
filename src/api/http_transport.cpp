#include "http_transport.h"

#include <wx/socket.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>

namespace
{
    std::optional<std::string> HeaderValue(const std::string& raw,
                                           const char* headerName)
    {
        const auto headerEnd = raw.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
            return std::nullopt;

        std::string expected(headerName);
        std::transform(expected.begin(), expected.end(), expected.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(std::tolower(character));
                       });
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
                    const auto valueStart = raw.find_first_not_of(" \t", colon + 1);
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
        catch (const std::exception&)
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
            catch (const std::exception&)
            {
                return std::nullopt;
            }

            const auto chunkStart = sizeEnd + 2;
            if (raw.size() < chunkStart + chunkSize + 2 ||
                raw.compare(chunkStart + chunkSize, 2, "\r\n") != 0)
                return std::nullopt;
            if (chunkSize == 0)
                return body;

            body.append(raw, chunkStart, chunkSize);
            if (firstChunkOnly)
                return body;
            offset = chunkStart + chunkSize + 2;
        }
    }

    bool ParseUrl(const std::string& url, std::string& host, int& port)
    {
        constexpr const char* prefix = "http://";
        if (url.rfind(prefix, 0) != 0)
            return false;
        auto value = url.substr(7);
        value = value.substr(0, value.find('/'));
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
        catch (const std::exception&)
        {
            return false;
        }
        return !host.empty() && port > 0 && port < 65536;
    }
}

HttpResponse HttpTransport::Send(const HttpRequest& request) const
{
    HttpResponse response;
    std::string host;
    int port = 0;
    if (!ParseUrl(request.url, host, port))
    {
        response.error = "Only http:// Controller URLs are supported";
        return response;
    }

    wxIPV4address address;
    address.Hostname(host);
    address.Service(port);
    wxSocketClient socket;
    const int timeoutMs = std::max(1, request.timeoutMs);
    socket.SetTimeout(static_cast<unsigned>((timeoutMs + 999) / 1000));
    if (!socket.Connect(address, true))
    {
        response.error = "Unable to connect to Mihomo Controller";
        return response;
    }

    const std::string rawRequest =
        request.method + " " + request.path + " HTTP/1.1\r\n" +
        "Host: " + host + "\r\n" +
        "Authorization: " + request.authorization + "\r\n" +
        "Content-Type: application/json\r\n" +
        "Connection: close\r\n" +
        "Content-Length: " + std::to_string(request.body.size()) +
        "\r\n\r\n" + request.body;

    std::size_t written = 0;
    while (written < rawRequest.size())
    {
        if (!socket.WaitForWrite(0, 100))
        {
            response.error = "Timed out sending Controller request";
            return response;
        }
        const auto remaining = rawRequest.size() - written;
        const auto writeSize = static_cast<wxUint32>(std::min<std::size_t>(
            remaining, std::numeric_limits<wxUint32>::max()));
        socket.Write(rawRequest.data() + written, writeSize);
        const auto count = socket.LastWriteCount();
        if (socket.Error() || count == 0)
        {
            response.error = "Unable to send Controller request";
            return response;
        }
        written += count;
    }

    std::string rawResponse;
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
        rawResponse.append(buffer, count);

        if (socket.Error() && rawResponse.find("\r\n\r\n") == std::string::npos)
        {
            response.error = "Unable to read Controller response";
            return response;
        }

        const auto headerEnd = rawResponse.find("\r\n\r\n");
        if (headerEnd != std::string::npos)
        {
            chunkedBody = DecodeChunkedBody(rawResponse, request.firstChunkOnly);
            if (chunkedBody)
                break;
        }
        const auto length = ContentLength(rawResponse);
        if (headerEnd != std::string::npos && length &&
            rawResponse.size() - headerEnd - 4 >= *length)
            break;
    }

    const auto headerEnd = rawResponse.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        const bool timedOut = std::chrono::steady_clock::now() >= deadline;
        response.error = rawResponse.empty()
                             ? (timedOut
                                    ? "Controller did not respond before timeout"
                                    : "Controller closed connection without a response")
                             : "Controller response did not contain an HTTP header";
        return response;
    }
    const auto firstLineEnd = rawResponse.find("\r\n");
    const auto statusStart = rawResponse.find(' ');
    if (firstLineEnd == std::string::npos || statusStart == std::string::npos ||
        statusStart > firstLineEnd)
    {
        response.error = "Invalid Controller status line";
        return response;
    }
    try
    {
        response.status = std::stoi(rawResponse.substr(statusStart + 1, 3));
    }
    catch (const std::exception&)
    {
        response.error = "Invalid Controller status code";
        return response;
    }

    response.body = rawResponse.substr(headerEnd + 4);
    if (HeaderValue(rawResponse, "transfer-encoding"))
    {
        if (!chunkedBody)
        {
            response.error = "Incomplete chunked Controller response";
            return response;
        }
        response.body = *chunkedBody;
    }
    if (!response.IsSuccess())
    {
        response.error = "Controller returned HTTP " + std::to_string(response.status);
        if (!response.body.empty())
            response.error += ": " + response.body;
    }
    return response;
}
