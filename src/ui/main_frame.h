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

class wxCheckBox;
class wxChoice;
class wxDataViewListCtrl;
class wxSimplebook;
class wxRadioBox;
class wxRadioButton;
class wxPanel;
class wxScrolledWindow;
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
        PageLogs,
        PageSettings,
        PageMihomo,
        PageCount
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
    void OnBrowseDataPath(wxCommandEvent& event);
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
    wxTextCtrl* logText_ = nullptr;
    wxStaticText* overviewStatus_ = nullptr;
    wxStaticText* overviewVersion_ = nullptr;
    wxStaticText* overviewConnections_ = nullptr;
    wxStaticText* overviewTraffic_ = nullptr;
    wxRadioBox* proxyGroups_ = nullptr;
    wxPanel* proxyGroupsPane_ = nullptr;
    wxPanel* proxyChoicesPane_ = nullptr;
    wxScrolledWindow* proxyChoicesScroll_ = nullptr;
    std::vector<wxRadioButton*> proxyChoiceButtons_;
    wxDataViewListCtrl* connectionTable_ = nullptr;
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
