#include "mihomo_sidecar.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
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

    void AppendLines(std::string& pending, const char* label, const char* data,
                     std::size_t size, const std::function<void(std::string)>& output)
    {
        pending.append(data, size);
        std::size_t newline = 0;
        while ((newline = pending.find('\n')) != std::string::npos)
        {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            output(std::string(label) + " " + line);
        }
    }

    void FlushLines(std::string& pending, const char* label,
                    const std::function<void(std::string)>& output)
    {
        if (!pending.empty())
        {
            output(std::string(label) + " " + pending);
            pending.clear();
        }
    }

#ifndef _WIN32
    void SetNonBlocking(int fd)
    {
        const int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0)
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    bool ReadFd(int fd, std::string& pending, const char* label,
                const std::function<void(std::string)>& output)
    {
        char buffer[4096];
        while (true)
        {
            const ssize_t count = read(fd, buffer, sizeof(buffer));
            if (count > 0)
            {
                AppendLines(pending, label, buffer, static_cast<std::size_t>(count), output);
                continue;
            }
            if (count == 0)
                return false;
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return true;
            return false;
        }
    }
#endif
}

wxDEFINE_EVENT(EVT_MIHOMO_SIDECAR, wxThreadEvent);

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
    RequestStop();
}

bool MihomoSidecar::Start(const std::string& corePath,
                          const std::string& dataPath,
                          const std::string& runtimeConfigPath,
                          wxEvtHandler* eventHandler,
                          std::string& error)
{
    if (IsRunning())
        return true;

    const auto core = ToWx(corePath);
    if (corePath.empty() || !wxFileName::FileExists(core))
        return Fail(error, "mihomo core path is empty or does not exist");
#ifndef _WIN32
    if (::access(core.c_str(), X_OK) != 0)
        return Fail(error, "mihomo core is not executable");
#endif
    if (dataPath.empty())
        return Fail(error, "WxClash data directory is empty");
    if (!MakeDirectory(ToWx(dataPath)))
        return Fail(error, "Unable to create WxClash data directory");
    if (runtimeConfigPath.empty() || !wxFileName::FileExists(ToWx(runtimeConfigPath)))
        return Fail(error, "mihomo config path is empty or does not exist");

    RequestStop();
    eventHandler_ = eventHandler;

    std::mutex startMutex;
    std::condition_variable startCondition;
    bool startFinished = false;
    std::string startError;
    worker_ = std::jthread(
        [this, corePath, dataPath, runtimeConfigPath, &startMutex, &startCondition,
         &startFinished, &startError](std::stop_token stopToken) {
            WorkerMain(stopToken, corePath, dataPath, runtimeConfigPath,
                       [&](std::string message) {
                           {
                               std::lock_guard<std::mutex> lock(startMutex);
                               startError = std::move(message);
                               startFinished = true;
                           }
                           startCondition.notify_one();
                       });
        });

    {
        std::unique_lock<std::mutex> lock(startMutex);
        startCondition.wait(lock, [&] { return startFinished; });
    }
    if (!startError.empty())
    {
        RequestStop();
        return Fail(error, std::move(startError));
    }
    return true;
}

void MihomoSidecar::RequestStop()
{
    if (worker_.joinable())
    {
        worker_.request_stop();
        worker_.join();
    }
    running_ = false;
}

void MihomoSidecar::PostEvent(MihomoSidecarEvent event)
{
    if (!eventHandler_)
        return;
    auto* wxEvent = new wxThreadEvent(EVT_MIHOMO_SIDECAR);
    wxEvent->SetPayload(std::move(event));
    wxQueueEvent(eventHandler_, wxEvent);
}

void MihomoSidecar::WorkerMain(std::stop_token stopToken,
                               std::string corePath,
                               std::string dataPath,
                               std::string runtimeConfigPath,
                               std::function<void(std::string)> reportStartError)
{
    const auto output = [this](std::string message) {
        PostEvent({MihomoSidecarEventType::Output, std::move(message)});
    };

#ifdef _WIN32
    HANDLE stdoutRead = nullptr;
    HANDLE stderrRead = nullptr;
    HANDLE stdoutWrite = nullptr;
    HANDLE stderrWrite = nullptr;
    PROCESS_INFORMATION process{};
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    if (!CreatePipe(&stdoutRead, &stdoutWrite, &security, 0) ||
        !CreatePipe(&stderrRead, &stderrWrite, &security, 0))
    {
        if (stdoutRead) CloseHandle(stdoutRead);
        if (stdoutWrite) CloseHandle(stdoutWrite);
        if (stderrRead) CloseHandle(stderrRead);
        if (stderrWrite) CloseHandle(stderrWrite);
        reportStartError("Unable to create mihomo output pipes");
        return;
    }
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);
    const auto quote = [](const std::wstring& value) {
        return std::wstring(L"\"") + value + L"\"";
    };
    const std::wstring core = wxString::FromUTF8(corePath).ToStdWstring();
    const std::wstring data = wxString::FromUTF8(dataPath).ToStdWstring();
    const std::wstring config = wxString::FromUTF8(runtimeConfigPath).ToStdWstring();
    std::wstring command = quote(core) + L" -d " + quote(data) + L" -f " + quote(config);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = stdoutWrite;
    startup.hStdError = stderrWrite;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
    {
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);
        CloseHandle(stderrRead);
        CloseHandle(stderrWrite);
        reportStartError("Unable to start mihomo sidecar");
        return;
    }
    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);
    running_ = true;
    reportStartError({});

    std::string stdoutPending;
    std::string stderrPending;
    const auto drain = [&](HANDLE handle, std::string& pending, const char* label) {
        char buffer[4096];
        DWORD available = 0;
        while (PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr) && available > 0)
        {
            DWORD count = 0;
            if (!ReadFile(handle, buffer, sizeof(buffer), &count, nullptr) || count == 0)
                break;
            AppendLines(pending, label, buffer, count, output);
        }
    };
    while (!stopToken.stop_requested() &&
           WaitForSingleObject(process.hProcess, 50) == WAIT_TIMEOUT)
    {
        drain(stdoutRead, stdoutPending, "[mihomo stdout]");
        drain(stderrRead, stderrPending, "[mihomo stderr]");
    }
    if (stopToken.stop_requested())
    {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
    }
    drain(stdoutRead, stdoutPending, "[mihomo stdout]");
    drain(stderrRead, stderrPending, "[mihomo stderr]");
    FlushLines(stdoutPending, "[mihomo stdout]", output);
    FlushLines(stderrPending, "[mihomo stderr]", output);
    CloseHandle(stdoutRead);
    CloseHandle(stderrRead);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
#else
    int stdoutPipe[2] = {-1, -1};
    int stderrPipe[2] = {-1, -1};
    if (pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0)
    {
        if (stdoutPipe[0] >= 0) close(stdoutPipe[0]);
        if (stdoutPipe[1] >= 0) close(stdoutPipe[1]);
        if (stderrPipe[0] >= 0) close(stderrPipe[0]);
        if (stderrPipe[1] >= 0) close(stderrPipe[1]);
        reportStartError("Unable to create mihomo output pipes");
        return;
    }
    const pid_t pid = fork();
    if (pid == 0)
    {
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[0]);
        close(stderrPipe[1]);
        execl(corePath.c_str(), corePath.c_str(), "-d", dataPath.c_str(),
              "-f", runtimeConfigPath.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    close(stdoutPipe[1]);
    close(stderrPipe[1]);
    if (pid < 0)
    {
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        reportStartError("Unable to start mihomo sidecar");
        return;
    }
    SetNonBlocking(stdoutPipe[0]);
    SetNonBlocking(stderrPipe[0]);
    running_ = true;
    reportStartError({});

    std::string stdoutPending;
    std::string stderrPending;
    bool stdoutOpen = true;
    bool stderrOpen = true;
    int status = 0;
    while (stdoutOpen || stderrOpen)
    {
        if (stopToken.stop_requested())
        {
            kill(pid, SIGTERM);
            for (int attempt = 0; attempt < 20; ++attempt)
            {
                if (waitpid(pid, &status, WNOHANG) == pid)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (waitpid(pid, &status, WNOHANG) == 0)
            {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
            }
        }

        pollfd fds[2] = {{stdoutPipe[0], POLLIN | POLLHUP, 0},
                         {stderrPipe[0], POLLIN | POLLHUP, 0}};
        const int result = poll(fds, 2, 100);
        if (result < 0 && errno != EINTR)
            break;
        if (stdoutOpen && (fds[0].revents & (POLLIN | POLLHUP)))
            stdoutOpen = ReadFd(stdoutPipe[0], stdoutPending, "[mihomo stdout]", output);
        if (stderrOpen && (fds[1].revents & (POLLIN | POLLHUP)))
            stderrOpen = ReadFd(stderrPipe[0], stderrPending, "[mihomo stderr]", output);

        if (!stopToken.stop_requested() && waitpid(pid, &status, WNOHANG) == pid &&
            !stdoutOpen && !stderrOpen)
            break;
    }
    if (!stopToken.stop_requested())
        waitpid(pid, &status, 0);
    ReadFd(stdoutPipe[0], stdoutPending, "[mihomo stdout]", output);
    ReadFd(stderrPipe[0], stderrPending, "[mihomo stderr]", output);
    FlushLines(stdoutPending, "[mihomo stdout]", output);
    FlushLines(stderrPending, "[mihomo stderr]", output);
    close(stdoutPipe[0]);
    close(stderrPipe[0]);
#endif

    running_ = false;
    PostEvent({MihomoSidecarEventType::Terminated, {}});
}
