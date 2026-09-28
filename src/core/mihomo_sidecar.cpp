#include "mihomo_sidecar.h"

#include "../api/mihomo_api_client.h"

#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#ifdef __unix__
#include <sys/stat.h>
#endif

#include <utility>

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
}

MihomoSidecar::~MihomoSidecar()
{
    Stop();
}

std::string MihomoSidecar::FindCore() const
{
    const auto executable = wxStandardPaths::Get().GetExecutablePath();
    const auto workingDirectory = wxGetCwd();
    wxFileName projectResources(wxFileName(executable).GetPath());
    projectResources.RemoveLastDir();
    projectResources.AppendDir("resources");
    const wxString candidates[] = {
        wxFileName(workingDirectory, "resources").GetFullPath(),
        wxFileName(wxFileName(executable).GetPath(), "resources").GetFullPath(),
        projectResources.GetFullPath(),
    };

    for (const auto& directory : candidates)
    {
        wxDir dir(directory);
        if (!dir.IsOpened())
            continue;

        wxString name;
        if (dir.GetFirst(&name, "mihomo*", wxDIR_FILES))
            return std::string((wxFileName(directory, name).GetFullPath()).utf8_str());
    }
    return {};
}

std::string MihomoSidecar::PrepareConfig(std::string& error) const
{
    const auto dataDirectory = wxStandardPaths::Get().GetUserLocalDataDir();
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

bool MihomoSidecar::Start(const MihomoApiClient& apiClient, std::string& error)
{
    if (IsRunning())
        return true;

    const auto core = FindCore();
    if (core.empty())
    {
        error = "mihomo core was not found under resources";
        return false;
    }

#ifdef __unix__
    // The checked-in resource may not have the executable bit after checkout.
    if (::chmod(core.c_str(), 0700) != 0)
    {
        error = "Unable to make mihomo executable";
        return false;
    }
#endif

    const auto config = PrepareConfig(error);
    if (config.empty())
        return false;

    const auto Quote = [](const wxString& value) {
        return wxString('"') + value + wxString('"');
    };
    const wxString command = Quote(ToWx(core)) + " -d " +
                             Quote(wxStandardPaths::Get().GetUserLocalDataDir()) +
                             " -f " + Quote(ToWx(config));

    pid_ = wxExecute(command, wxEXEC_ASYNC);
    if (pid_ <= 0)
    {
        error = "Unable to start mihomo sidecar";
        return false;
    }

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
