#include "mihomo_api_client.h"

#include <wx/socket.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace
{
    std::optional<std::size_t> ContentLength(const std::string& raw)
    {
        const auto headerEnd = raw.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
            return std::nullopt;
        const auto name = raw.find("Content-Length:");
        if (name == std::string::npos || name > headerEnd)
            return std::nullopt;
        const auto valueStart = raw.find_first_not_of(" \t", name + 15);
        const auto valueEnd = raw.find("\r\n", valueStart);
        try
        {
            return std::stoull(raw.substr(valueStart, valueEnd - valueStart));
        }
        catch (...)
        {
            return std::nullopt;
        }
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
    socket.SetTimeout(static_cast<unsigned>(config_.timeoutMs / 1000));
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
    socket.Write(request.data(), request.size());
    if (socket.Error())
    {
        response.error = "Unable to send Controller request";
        return response;
    }

    std::string raw;
    char buffer[4096];
    while (socket.WaitForRead(0, config_.timeoutMs))
    {
        socket.Read(buffer, sizeof(buffer));
        const auto count = socket.LastCount();
        if (count == 0)
            break;
        raw.append(buffer, count);

        const auto headerEnd = raw.find("\r\n\r\n");
        const auto length = ContentLength(raw);
        if (headerEnd != std::string::npos && length &&
            raw.size() - headerEnd - 4 >= *length)
            break;
    }

    // Keep the request secret out of logs. The complete response is sent to
    // the application's Logs page through the debug callback below.
    if (debugCallback_)
        debugCallback_("Mihomo API raw response (" + method + " " + path + "):\n" + raw);

    const auto headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        response.error = "Invalid Controller response";
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
    response.ok = response.status >= 200 && response.status < 300;
    if (!response.ok)
        response.error = "Controller returned HTTP " + std::to_string(response.status);
    return response;
}
