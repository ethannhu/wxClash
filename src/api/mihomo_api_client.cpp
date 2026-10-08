#include "mihomo_api_client.h"

#include "http_transport.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <utility>

namespace
{
    constexpr int kStreamingSnapshotTimeoutMs = 2500;

    bool IsStreamingSnapshot(const std::string& method,
                             const std::string& path)
    {
        return method == "GET" &&
               (path == "/traffic" || path == "/memory" ||
                path == "/connections");
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
            {
                encoded.push_back(static_cast<char>(character));
            }
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
    const bool streamingSnapshot = IsStreamingSnapshot(method, path);
    const int timeoutMs = streamingSnapshot
                              ? std::max(config_.timeoutMs,
                                         kStreamingSnapshotTimeoutMs)
                              : config_.timeoutMs;
    const HttpRequest request{
        config_.baseUrl,
        method,
        path,
        "Bearer " + config_.secret,
        body,
        timeoutMs,
        streamingSnapshot};
    const auto transportResponse = transport_.Send(request);

    MihomoApiResponse response;
    response.status = transportResponse.status;
    response.body = transportResponse.body;
    response.error = transportResponse.error;
    response.ok = transportResponse.error.empty() &&
                  transportResponse.IsSuccess();
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
