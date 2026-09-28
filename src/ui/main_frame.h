#pragma once

#include <wx/frame.h>
#include <wx/timer.h>

#include "../api/mihomo_api_client.h"
#include "../config/app_config.h"
#include "../core/mihomo_sidecar.h"

class wxCheckBox;
class wxChoice;
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
    void OnBrowseCore(wxCommandEvent& event);
    void OnBrowseDataPath(wxCommandEvent& event);
    void OnImportConfig(wxCommandEvent& event);
    void OnSaveMihomoConfig(wxCommandEvent& event);

    wxSimplebook* book_ = nullptr;
    wxChoice* modeChoice_ = nullptr;
    wxTextCtrl* logText_ = nullptr;
    wxTextCtrl* corePathText_ = nullptr;
    wxTextCtrl* dataPathText_ = nullptr;
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
    wxCheckBox* allowLanCheck_ = nullptr;
    wxCheckBox* ipv6Check_ = nullptr;
    wxCheckBox* tunEnableCheck_ = nullptr;
    wxCheckBox* dnsEnableCheck_ = nullptr;
    std::string corePath_;
    std::string dataPath_;
    MihomoConfig mihomoConfig_;
    MihomoApiClient apiClient_;
    MihomoSidecar mihomoSidecar_;
    wxTimer sidecarOutputTimer_;

    wxDECLARE_EVENT_TABLE();
};
