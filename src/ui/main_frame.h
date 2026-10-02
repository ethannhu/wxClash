#pragma once

#include <wx/frame.h>
#include <wx/timer.h>

#include <cstdint>
#include <cstddef>

#include "../api/mihomo_api_client.h"
#include "../config/app_config.h"
#include "../core/mihomo_sidecar.h"

class wxCheckBox;
class wxChoice;
class wxDataViewListCtrl;
class wxListBox;
class wxSimplebook;
class wxSizer;
class wxStaticText;
class wxTextCtrl;
class wxWindow;

class MainFrame final : public wxFrame
{
public:
    MainFrame();
    ~MainFrame() override;

private:
    enum PageId
    {
        PageOverview,
        PageProxies,
        PageConnections,
        PageRules,
        PageLogs,
        PageSettings,
        PageCount
    };

    void BuildNavigation(wxSizer* parentSizer);
    void BuildPages();
    void AddNavigationButton(wxWindow* parent, wxSizer* sizer,
                             const wxString& label, PageId page);

    void OnNavigation(wxCommandEvent& event);
    void OnModeChanged(wxCommandEvent& event);
    void OnConnectApi(wxCommandEvent& event);
    void OnSidecarOutput(wxTimerEvent& event);
    void OnMonitorTimer(wxTimerEvent& event);
    void OnBrowseCore(wxCommandEvent& event);
    void OnBrowseDataPath(wxCommandEvent& event);
    void OnBrowseConfig(wxCommandEvent& event);
    void OnSaveMihomoConfig(wxCommandEvent& event);
    void UpdateMihomoControls();
    void RefreshCoreData();
    void RefreshProxies();
    void RefreshRules();
    void AppendLog(const wxString& message);
    void ApplyPollingSettings();

    wxSimplebook* book_ = nullptr;
    wxChoice* modeChoice_ = nullptr;
    wxTextCtrl* logText_ = nullptr;
    wxStaticText* downloadMetric_ = nullptr;
    wxStaticText* uploadMetric_ = nullptr;
    wxStaticText* activeConnectionsMetric_ = nullptr;
    wxStaticText* memoryMetric_ = nullptr;
    wxStaticText* trafficSummary_ = nullptr;
    wxDataViewListCtrl* proxyTable_ = nullptr;
    wxListBox* proxyGroups_ = nullptr;
    wxDataViewListCtrl* connectionTable_ = nullptr;
    wxDataViewListCtrl* ruleTable_ = nullptr;
    wxTextCtrl* corePathText_ = nullptr;
    wxTextCtrl* dataPathText_ = nullptr;
    wxTextCtrl* configPathText_ = nullptr;
    wxChoice* mihomoModeChoice_ = nullptr;
    wxChoice* mihomoLogLevelChoice_ = nullptr;
    wxChoice* tunStackChoice_ = nullptr;
    wxChoice* dnsModeChoice_ = nullptr;
    wxTextCtrl* mixedPortText_ = nullptr;
    wxTextCtrl* httpPortText_ = nullptr;
    wxTextCtrl* socksPortText_ = nullptr;
    wxTextCtrl* controllerText_ = nullptr;
    wxTextCtrl* secretText_ = nullptr;
    wxTextCtrl* nameserverText_ = nullptr;
    wxChoice* pollingIntervalChoice_ = nullptr;
    wxChoice* logLengthChoice_ = nullptr;
    wxCheckBox* allowLanCheck_ = nullptr;
    wxCheckBox* ipv6Check_ = nullptr;
    wxCheckBox* tunEnableCheck_ = nullptr;
    wxCheckBox* dnsEnableCheck_ = nullptr;
    std::string corePath_;
    std::string dataPath_;
    std::string configPath_;
    MihomoConfig mihomoConfig_;
    MihomoApiClient apiClient_;
    MihomoSidecar mihomoSidecar_;
    wxTimer sidecarOutputTimer_;
    wxTimer monitorTimer_;
    std::uint64_t lastDownloadTotal_ = 0;
    std::uint64_t lastUploadTotal_ = 0;
    bool hasTrafficSample_ = false;
    bool apiConnected_ = false;
    int pollingIntervalMs_ = 2000;
    std::size_t maxLogLength_ = 100000;

    wxDECLARE_EVENT_TABLE();
};
