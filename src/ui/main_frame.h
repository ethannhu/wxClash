#pragma once

#include <wx/frame.h>
#include <wx/timer.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "../api/mihomo_api_client.h"
#include "../config/app_config.h"
#include "../core/mihomo_sidecar.h"
#include "pages.h"

class wxSimplebook;
class wxRadioButton;
class wxSizer;
class wxWindow;

class MainFrame final : public wxFrame
{
public:
    MainFrame();
    ~MainFrame() override;

private:
    enum class PageId
    {
        Overview,
        Proxies,
        Connections,
        Logs,
        Settings,
        Mihomo,
        Count
    };

    void BuildNavigation(wxSizer* parentSizer);
    void BuildPages();
    void AddNavigationButton(wxWindow* parent, wxSizer* sizer,
                             const wxString& label, PageId page);

    void OnNavigation(wxCommandEvent& event);
    void OnModeChanged(wxCommandEvent& event);
    void OnConnectApi(wxCommandEvent& event);
    void OnDisconnectApi(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnSidecarOutput(wxTimerEvent& event);
    void OnMonitorTimer(wxTimerEvent& event);
    void OnProxyGroupSelected(wxCommandEvent& event);
    void OnProxySelected(wxCommandEvent& event);
    void OnBrowseCore(wxCommandEvent& event);
    void OnBrowseConfig(wxCommandEvent& event);
    bool SaveMihomoSettings(std::string& error);
    bool PrepareRuntimeConfig(std::string& runtimePath, std::string& error);
    void UpdateMihomoControls();
    void RefreshCoreData();
    void RefreshProxies();
    void RefreshProxyGroup();
    void PopulateProxyChoices();
    void SelectProxy(const std::string& proxyName);
    void AppendLog(const wxString& message);

    wxSimplebook* book_ = nullptr;
    OverviewPage* overviewPage_ = nullptr;
    ProxyPage* proxyPage_ = nullptr;
    ConnectionsPage* connectionsPage_ = nullptr;
    LogsPage* logsPage_ = nullptr;
    SettingsPage* settingsPage_ = nullptr;
    MihomoPage* mihomoPage_ = nullptr;
    std::string corePath_;
    std::string dataPath_;
    std::string configPath_;
    MihomoConfig mihomoConfig_;
    MihomoApiClient apiClient_;
    MihomoSidecar mihomoSidecar_;
    wxTimer sidecarOutputTimer_;
    wxTimer monitorTimer_;
    bool apiConnected_ = false;
    bool closing_ = false;
    std::size_t maxLogLength_ = 100000;
    std::map<std::string, std::vector<std::string>> proxyGroupMembers_;
    std::map<std::string, std::string> proxyCurrentSelection_;
    std::vector<std::string> proxyGroupNames_;
    std::vector<std::string> proxyChoiceNames_;
    nlohmann::json proxyData_;
    std::string selectedProxyGroup_;
    bool updatingProxyTable_ = false;
    bool updatingProxyGroups_ = false;

    wxDECLARE_EVENT_TABLE();
};
