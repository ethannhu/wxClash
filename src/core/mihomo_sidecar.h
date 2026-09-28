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

    bool Start(const MihomoApiClient& apiClient, std::string& error);
    void Stop();
    bool IsRunning() const { return pid_ > 0; }

private:
    std::string FindCore() const;
    std::string PrepareConfig(std::string& error) const;

    long pid_ = -1;
};
