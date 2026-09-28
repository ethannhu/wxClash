#include "mihomo_sidecar.h"

#include "../api/mihomo_api_client.h"

#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#ifdef __unix__
#include <unistd.h>
#endif

#include <utility>
#include <iostream>

namespace
{
    constexpr int kReadyAttempts = 30;
    constexpr unsigned kReadyIntervalMs = 100;

    wxString ToWx(const std::string& value)
    {
        return wxString::FromUTF8(value);
    }

    bool MakeDirectory(const wxString& path)
    {
        return wxDir::Exists(path) || wxFileName::Mkdir(path, 0700, wxPATH_MKDIR_FULL);
    }

    bool Fail(std::string& error, std::string message)
    {
        error = std::move(message);
        std::cerr << "[WxClash] " << error << std::endl;
        return false;
    }
}

std::string MihomoSidecar::DefaultDataPath()
{
#ifdef __WXMSW__
    const auto path = wxFileName(wxStandardPaths::Get().GetUserConfigDir(),
                                 "WxClash").GetFullPath();
#else
    const auto path = wxFileName(wxGetHomeDir(), ".wxclash").GetFullPath();
#endif
    return std::string(path.utf8_str());
}

MihomoSidecar::~MihomoSidecar()
{
    Stop();
}

std::string MihomoSidecar::PrepareConfig(const std::string& dataPath,
                                         std::string& error) const
{
    const auto dataDirectory = ToWx(dataPath);
    if (dataDirectory.empty())
    {
        error = "WxClash data directory is empty";
        return {};
    }
    if (!MakeDirectory(dataDirectory))
    {
        error = "Unable to create WxClash data directory";
        return {};
    }

    const wxFileName configPath(dataDirectory, "mihomo-runtime.yaml");
    const wxString config =
        "mode: rule\n"
        "log-level: info\n"
        "allow-lan: false\n"
        "external-controller: 127.0.0.1:9090\n"
        "secret: \"123456\"\n"
        "rules:\n"
        "  - MATCH,DIRECT\n";

    wxFFile file(configPath.GetFullPath(), "w");
    if (!file.IsOpened() || !file.Write(config) || !file.Close())
    {
        error = "Unable to write mihomo runtime config";
        return {};
    }
    return std::string(configPath.GetFullPath().utf8_str());
}

bool MihomoSidecar::Start(const std::string& corePath,
                          const std::string& dataPath,
                          const MihomoApiClient& apiClient,
                          std::string& error)
{
    if (IsRunning())
        return true;

    const auto core = ToWx(corePath);
    if (corePath.empty() || !wxFileName::FileExists(core))
        return Fail(error, "mihomo core path is empty or does not exist");

#ifdef __unix__
    if (::access(core.c_str(), X_OK) != 0)
        return Fail(error, "mihomo core is not executable");
#endif

    const auto config = PrepareConfig(dataPath, error);
    if (config.empty())
        return false;

    const auto Quote = [](const wxString& value) {
        return wxString('"') + value + wxString('"');
    };
    const wxString command = Quote(core) + " -d " +
                             Quote(ToWx(dataPath)) +
                             " -f " + Quote(ToWx(config));

    pid_ = wxExecute(command, wxEXEC_ASYNC);
    if (pid_ <= 0)
        return Fail(error, "Unable to start mihomo sidecar");

    MihomoApiClient probe = apiClient;
    probe.SetTimeoutMs(300);
    for (int attempt = 0; attempt < kReadyAttempts; ++attempt)
    {
        const auto response = probe.GetVersion();
        if (response.ok)
            return true;

        wxMilliSleep(kReadyIntervalMs);
    }

    error = "mihomo sidecar did not become ready";
    std::cerr << "[WxClash] " << error << std::endl;
    Stop();
    return false;
}

void MihomoSidecar::Stop()
{
    if (pid_ <= 0)
        return;

    wxKill(pid_, wxSIGTERM);
    wxMilliSleep(100);
    wxKill(pid_, wxSIGKILL);
    pid_ = -1;
}
