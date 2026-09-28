#include "mihomo_sidecar.h"

#include "../api/mihomo_api_client.h"

#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/process.h>
#include <wx/stdpaths.h>
#include <wx/stream.h>
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

    process_ = std::make_unique<wxProcess>(nullptr);
    process_->Redirect();
    pid_ = wxExecute(command, wxEXEC_ASYNC, process_.get());
    if (pid_ <= 0)
    {
        process_.reset();
        return Fail(error, "Unable to start mihomo sidecar");
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
    std::cerr << "[WxClash] " << error << std::endl;
    Stop();
    return false;
}

void MihomoSidecar::Stop()
{
    if (pid_ <= 0)
        return;

    PollOutput();
    wxKill(pid_, wxSIGTERM);
    wxMilliSleep(100);
    PollOutput();
    wxKill(pid_, wxSIGKILL);
    PollOutput();
    FlushPendingOutput();
    process_.reset();
    pid_ = -1;
}

void MihomoSidecar::DrainStream(wxInputStream* stream, std::string& pending,
                                const char* label)
{
    if (!stream)
        return;

    char buffer[4096];
    while (stream->CanRead())
    {
        stream->Read(buffer, sizeof(buffer));
        const auto count = stream->LastRead();
        if (count == 0)
            break;
        pending.append(buffer, count);

        std::size_t newline = 0;
        while ((newline = pending.find('\n')) != std::string::npos)
        {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (outputCallback_)
                outputCallback_(std::string(label) + " " + line);
        }
    }
}

void MihomoSidecar::FlushPendingOutput()
{
    if (outputCallback_ && !stdoutPending_.empty())
    {
        outputCallback_("[mihomo stdout] " + stdoutPending_);
        stdoutPending_.clear();
    }
    if (outputCallback_ && !stderrPending_.empty())
    {
        outputCallback_("[mihomo stderr] " + stderrPending_);
        stderrPending_.clear();
    }
}

void MihomoSidecar::PollOutput()
{
    if (!process_)
        return;

    DrainStream(process_->GetInputStream(), stdoutPending_, "[mihomo stdout]");
    DrainStream(process_->GetErrorStream(), stderrPending_, "[mihomo stderr]");
}
