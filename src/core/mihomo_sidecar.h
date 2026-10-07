#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <wx/process.h>

class MihomoApiClient;
class wxInputStream;
class MihomoSidecarProcess;

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
               const std::string& configPath,
               const MihomoApiClient& apiClient,
               std::string& error);
    void Stop();
    void PollOutput();
    void SetOutputCallback(std::function<void(const std::string&)> callback)
    {
        outputCallback_ = std::move(callback);
    }
    bool IsRunning() const { return pid_ > 0; }

private:
    friend class MihomoSidecarProcess;

    void OnProcessTerminated(long pid, int status);
    void DrainStream(wxInputStream* stream, std::string& pending,
                     const char* label, bool mirrorToConsole);
    void FlushPendingOutput();

    long pid_ = -1;
    MihomoSidecarProcess* process_ = nullptr;
    std::function<void(const std::string&)> outputCallback_;
    std::string stdoutPending_;
    std::string stderrPending_;
};
