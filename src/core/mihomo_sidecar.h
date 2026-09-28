#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <wx/process.h>

class MihomoApiClient;
struct MihomoConfig;
class wxInputStream;

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
               const MihomoConfig& mihomoConfig,
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
    std::string PrepareConfig(const std::string& dataPath,
                              const std::string& configPath,
                              const MihomoConfig& mihomoConfig,
                              std::string& error) const;
    void DrainStream(wxInputStream* stream, std::string& pending,
                     const char* label);
    void FlushPendingOutput();

    long pid_ = -1;
    std::unique_ptr<wxProcess> process_;
    std::function<void(const std::string&)> outputCallback_;
    std::string stdoutPending_;
    std::string stderrPending_;
};
