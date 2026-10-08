#pragma once

#include <vector>

#include <wx/panel.h>

enum class ControlId : int
{
    ConnectApi = wxID_HIGHEST + 1,
    BrowseCore,
    BrowseConfig,
    SidecarOutputTimer,
    MonitorTimer,
    DisconnectApi,
    ProxyGroup,
    ProxyChoice
};

class wxCheckBox;
class wxChoice;
class wxDataViewListCtrl;
class wxRadioBox;
class wxRadioButton;
class wxScrolledWindow;
class wxStaticText;
class wxTextCtrl;

class OverviewPage final : public wxPanel
{
public:
    explicit OverviewPage(wxWindow* parent);

    wxStaticText* status = nullptr;
    wxStaticText* version = nullptr;
    wxStaticText* connections = nullptr;
    wxStaticText* traffic = nullptr;
};

class ProxyPage final : public wxPanel
{
public:
    explicit ProxyPage(wxWindow* parent);

    wxRadioBox* groups = nullptr;
    wxPanel* groupsPane = nullptr;
    wxScrolledWindow* choicesScroll = nullptr;
    std::vector<wxRadioButton*> choiceButtons;
};

class ConnectionsPage final : public wxPanel
{
public:
    explicit ConnectionsPage(wxWindow* parent);

    wxDataViewListCtrl* table = nullptr;
};

class LogsPage final : public wxPanel
{
public:
    explicit LogsPage(wxWindow* parent);

    wxTextCtrl* text = nullptr;
};

class SettingsPage final : public wxPanel
{
public:
    explicit SettingsPage(wxWindow* parent);

    wxTextCtrl* corePath = nullptr;
    wxTextCtrl* configPath = nullptr;
    wxChoice* logLength = nullptr;
};

class MihomoPage final : public wxPanel
{
public:
    explicit MihomoPage(wxWindow* parent);

    wxChoice* mode = nullptr;
    wxChoice* logLevel = nullptr;
    wxChoice* tunStack = nullptr;
    wxChoice* dnsMode = nullptr;
    wxTextCtrl* mixedPort = nullptr;
    wxTextCtrl* httpPort = nullptr;
    wxTextCtrl* socksPort = nullptr;
    wxTextCtrl* controller = nullptr;
    wxTextCtrl* secret = nullptr;
    wxTextCtrl* nameservers = nullptr;
    wxCheckBox* allowLan = nullptr;
    wxCheckBox* ipv6 = nullptr;
    wxCheckBox* tunEnable = nullptr;
    wxCheckBox* dnsEnable = nullptr;
};
