#pragma once

#include <string>

struct HttpRequest
{
    std::string url;
    std::string method;
    std::string path;
    std::string authorization;
    std::string body;
    int timeoutMs = 3000;
    bool firstChunkOnly = false;
};

struct HttpResponse
{
    int status = 0;
    std::string body;
    std::string error;

    bool IsSuccess() const noexcept
    {
        return status >= 200 && status < 300;
    }
};

class HttpTransport final
{
public:
    HttpResponse Send(const HttpRequest& request) const;
};
