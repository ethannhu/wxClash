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

bool MihomoSidecar::Start(const std::string& corePath,
                          const std::string& dataPath,
                          const std::string& configPath,
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

    if (dataPath.empty())
        return Fail(error, "WxClash data directory is empty");
    if (!MakeDirectory(ToWx(dataPath)))
        return Fail(error, "Unable to create WxClash data directory");
    if (configPath.empty() || !wxFileName::FileExists(ToWx(configPath)))
        return Fail(error, "mihomo config path is empty or does not exist");

    const auto Quote = [](const wxString& value) {
        return wxString('"') + value + wxString('"');
    };
    const wxString command = Quote(core) + " -d " +
                             Quote(ToWx(dataPath)) +
                             " -f " + Quote(ToWx(configPath));

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
                                const char* label, bool mirrorToConsole)
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
            const auto message = std::string(label) + " " + line;
            if (outputCallback_)
                outputCallback_(message);
            if (mirrorToConsole)
                std::cerr << message << std::endl;
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
    if (!stderrPending_.empty())
    {
        const auto message = "[mihomo stderr] " + stderrPending_;
        if (outputCallback_)
            outputCallback_(message);
        std::cerr << message << std::endl;
        stderrPending_.clear();
    }
}

void MihomoSidecar::PollOutput()
{
    if (!process_)
        return;

    DrainStream(process_->GetInputStream(), stdoutPending_, "[mihomo stdout]", false);
    DrainStream(process_->GetErrorStream(), stderrPending_, "[mihomo stderr]", true);
}
