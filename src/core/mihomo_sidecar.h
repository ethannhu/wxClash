#pragma once

#include <string>

class MihomoApiClient;

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
               const MihomoApiClient& apiClient,
               std::string& error);
    void Stop();
    bool IsRunning() const { return pid_ > 0; }

private:
    std::string PrepareConfig(const std::string& dataPath,
                              std::string& error) const;

    long pid_ = -1;
};
