#pragma once

#include <wx/frame.h>

#include "../api/mihomo_api_client.h"
#include "../core/mihomo_sidecar.h"

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

    wxSimplebook* book_ = nullptr;
    wxChoice* modeChoice_ = nullptr;
    wxTextCtrl* logText_ = nullptr;
    MihomoApiClient apiClient_;
    MihomoSidecar mihomoSidecar_;

    wxDECLARE_EVENT_TABLE();
};
