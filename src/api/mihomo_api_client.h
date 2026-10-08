#pragma once

#include <string>
#include <utility>

#include "http_transport.h"

struct MihomoApiConfig
{
    std::string baseUrl = "http://127.0.0.1:9090";
    std::string secret = "123456";
    int timeoutMs = 3000;
};

struct MihomoApiResponse
{
    bool ok = false;
    int status = 0;
    std::string body;
    std::string error;
};

class MihomoApiClient final
{
public:
    explicit MihomoApiClient(MihomoApiConfig config = {});

    void SetBaseUrl(std::string baseUrl) { config_.baseUrl = std::move(baseUrl); }
    void SetSecret(std::string secret) { config_.secret = std::move(secret); }
    void SetTimeoutMs(int timeoutMs) { config_.timeoutMs = timeoutMs; }

    MihomoApiResponse Request(const std::string& method,
                              const std::string& path,
                              const std::string& body = {}) const;

    MihomoApiResponse GetVersion() const { return Request("GET", "/version"); }
    MihomoApiResponse GetConfig() const { return Request("GET", "/configs"); }
    MihomoApiResponse GetProxies() const { return Request("GET", "/proxies"); }
    MihomoApiResponse GetProxy(const std::string& group) const;
    MihomoApiResponse SelectProxy(const std::string& group,
                                  const std::string& proxy) const;
    MihomoApiResponse GetTraffic() const { return Request("GET", "/traffic"); }
    MihomoApiResponse GetMemory() const { return Request("GET", "/memory"); }
    MihomoApiResponse GetConnections() const { return Request("GET", "/connections"); }
    MihomoApiResponse Restart() const { return Request("POST", "/restart"); }

private:
    MihomoApiConfig config_;
    HttpTransport transport_;
};
