#include "mihomo_api_service.h"

#include <wx/event.h>

#include <chrono>
#include <utility>

namespace
{
    constexpr int kProbeAttempts = 100;
    constexpr int kProbeIntervalMs = 100;
}

MihomoApiService::MihomoApiService(MihomoApiConfig config, wxEvtHandler* resultHandler)
    : client_(std::move(config)), resultHandler_(resultHandler),
      worker_(&MihomoApiService::WorkerMain, this)
{
}

MihomoApiService::~MihomoApiService() { Shutdown(); }

std::uint64_t MihomoApiService::Enqueue(Task task)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_)
        return 0;
    const auto requestId = nextRequestId_++;
    tasks_.emplace_back(requestId, std::move(task));
    condition_.notify_one();
    return requestId;
}

std::uint64_t MihomoApiService::Connect()
{
    return Enqueue([this](std::uint64_t requestId) {
        auto probe = client_;
        probe.SetTimeoutMs(300);
        MihomoApiResponse response;
        for (int attempt = 0; attempt < kProbeAttempts; ++attempt)
        {
            response = probe.GetVersion();
            if (response.ok)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(kProbeIntervalMs));
        }
        PostResult({MihomoApiOperation::Connect, requestId, {}, {}, std::move(response)});
    });
}

std::uint64_t MihomoApiService::GetVersion()
{
    return Enqueue([this](std::uint64_t id) {
        PostResult({MihomoApiOperation::Version, id, {}, {}, client_.GetVersion()});
    });
}

std::uint64_t MihomoApiService::GetConnections()
{
    return Enqueue([this](std::uint64_t id) {
        PostResult({MihomoApiOperation::Connections, id, {}, {}, client_.GetConnections()});
    });
}

std::uint64_t MihomoApiService::GetTraffic()
{
    return Enqueue([this](std::uint64_t id) {
        PostResult({MihomoApiOperation::Traffic, id, {}, {}, client_.GetTraffic()});
    });
}

std::uint64_t MihomoApiService::GetProxies()
{
    return Enqueue([this](std::uint64_t id) {
        PostResult({MihomoApiOperation::Proxies, id, {}, {}, client_.GetProxies()});
    });
}

std::uint64_t MihomoApiService::GetProxy(std::string group)
{
    return Enqueue([this, group = std::move(group)](std::uint64_t id) {
        PostResult({MihomoApiOperation::ProxyGroup, id, group, {}, client_.GetProxy(group)});
    });
}

std::uint64_t MihomoApiService::SelectProxy(std::string group, std::string proxy)
{
    return Enqueue([this, group = std::move(group), proxy = std::move(proxy)](std::uint64_t id) {
        PostResult({MihomoApiOperation::SelectProxy, id, group, proxy,
                    client_.SelectProxy(group, proxy)});
    });
}

void MihomoApiService::Configure(MihomoApiConfig config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stopping_ && tasks_.empty())
        client_ = MihomoApiClient(std::move(config));
}

void MihomoApiService::WorkerMain()
{
    while (true)
    {
        std::pair<std::uint64_t, Task> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty())
                return;
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        task.second(task.first);
    }
}

void MihomoApiService::PostResult(MihomoApiResult result)
{
    if (!resultHandler_)
        return;
    auto* event = new wxThreadEvent(wxEVT_THREAD);
    event->SetPayload(std::move(result));
    wxQueueEvent(resultHandler_, event);
}

void MihomoApiService::Shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_)
            return;
        stopping_ = true;
        tasks_.clear();
    }
    condition_.notify_one();
    if (worker_.joinable())
        worker_.join();
    resultHandler_ = nullptr;
}
