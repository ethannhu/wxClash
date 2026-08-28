#pragma once

#include <wx/frame.h>

class wxChoice;
class wxSimplebook;
class wxSizer;
class wxStaticText;
class wxWindow;

class MainFrame final : public wxFrame
{
public:
    MainFrame();

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

    wxSimplebook* book_ = nullptr;
    wxChoice* modeChoice_ = nullptr;

    wxDECLARE_EVENT_TABLE();
};
