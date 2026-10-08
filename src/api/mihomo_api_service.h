#pragma once

#include "mihomo_api_client.h"

#include <wx/event.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

wxDECLARE_EVENT(EVT_MIHOMO_API, wxThreadEvent);

enum class MihomoApiOperation { Connect, Version, Connections, Traffic, Proxies, ProxyGroup, SelectProxy };

struct MihomoApiResult
{
    MihomoApiOperation operation;
    std::uint64_t requestId = 0;
    std::string group;
    std::string proxy;
    MihomoApiResponse response;
};

class MihomoApiService final
{
public:
    MihomoApiService(MihomoApiConfig config, wxEvtHandler* resultHandler);
    ~MihomoApiService();
    MihomoApiService(const MihomoApiService&) = delete;
    MihomoApiService& operator=(const MihomoApiService&) = delete;

    std::uint64_t Connect();
    std::uint64_t GetVersion();
    std::uint64_t GetConnections();
    std::uint64_t GetTraffic();
    std::uint64_t GetProxies();
    std::uint64_t GetProxy(std::string group);
    std::uint64_t SelectProxy(std::string group, std::string proxy);
    void Configure(MihomoApiConfig config);
    void Shutdown();

private:
    using Task = std::function<void(std::uint64_t)>;
    std::uint64_t Enqueue(Task task);
    void WorkerMain();
    void PostResult(MihomoApiResult result);

    MihomoApiClient client_;
    wxEvtHandler* resultHandler_ = nullptr;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::pair<std::uint64_t, Task>> tasks_;
    std::thread worker_;
    std::uint64_t nextRequestId_ = 1;
    bool stopping_ = false;
};
