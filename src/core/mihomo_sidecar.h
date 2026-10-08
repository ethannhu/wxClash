#pragma once

#include <wx/event.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>

wxDECLARE_EVENT(EVT_MIHOMO_SIDECAR, wxThreadEvent);

enum class MihomoSidecarEventType
{
    Output,
    Terminated,
};

struct MihomoSidecarEvent
{
    MihomoSidecarEventType type;
    std::string message;
};

class wxEvtHandler;

class MihomoSidecar final
{
public:
    MihomoSidecar() = default;
    ~MihomoSidecar();

    MihomoSidecar(const MihomoSidecar&) = delete;
    MihomoSidecar& operator=(const MihomoSidecar&) = delete;

    static std::string DefaultDataPath();

    bool Start(const std::string& corePath,
               const std::string& dataPath,
               const std::string& runtimeConfigPath,
               wxEvtHandler* eventHandler,
               std::string& error);
    void RequestStop();
    bool IsRunning() const { return running_.load(); }

private:
    void WorkerMain(std::stop_token stopToken,
                    std::string corePath,
                    std::string dataPath,
                    std::string runtimeConfigPath,
                    std::function<void(std::string)> reportStartError);
    void PostEvent(MihomoSidecarEvent event);

    wxEvtHandler* eventHandler_ = nullptr;
    std::jthread worker_;
    std::atomic<bool> running_ = false;
};
