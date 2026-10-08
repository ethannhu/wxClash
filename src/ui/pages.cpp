#include "pages.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/radiobox.h>
#include <wx/radiobut.h>
#include <wx/scrolwin.h>
#include <wx/srchctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace
{
    constexpr int kSpacing = 8;

    wxBoxSizer* PageSizer(wxPanel* page, const wxString& title)
    {
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        auto* heading = new wxStaticText(page, wxID_ANY, title);
        heading->SetFont(heading->GetFont().Bold().Scale(1.35));
        sizer->Add(heading, 0, wxALL, kSpacing);
        page->SetSizer(sizer);
        return sizer;
    }

    void AddTableColumn(wxDataViewListCtrl* table, const wxString& title,
                        int width)
    {
        table->AppendTextColumn(title, wxDATAVIEW_CELL_INERT, width,
                                wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    }
}

OverviewPage::OverviewPage(wxWindow* parent)
    : wxPanel(parent)
{
    auto* sizer = PageSizer(this, "Overview");
    auto* info = new wxFlexGridSizer(2, kSpacing, kSpacing);
    status = new wxStaticText(this, wxID_ANY, "Not connected");
    version = new wxStaticText(this, wxID_ANY, "Mihomo version: -");
    connections = new wxStaticText(this, wxID_ANY, "Connections: -");
    traffic = new wxStaticText(this, wxID_ANY, "Traffic: -");
    info->Add(new wxStaticText(this, wxID_ANY, "Status"));
    info->Add(status, 1, wxEXPAND);
    info->Add(new wxStaticText(this, wxID_ANY, "Version"));
    info->Add(version, 1, wxEXPAND);
    info->Add(new wxStaticText(this, wxID_ANY, "Connections"));
    info->Add(connections, 1, wxEXPAND);
    info->Add(new wxStaticText(this, wxID_ANY, "Traffic"));
    info->Add(traffic, 1, wxEXPAND);
    info->AddGrowableCol(1, 1);
    sizer->Add(info, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, kSpacing);

    auto* shortcuts = new wxBoxSizer(wxHORIZONTAL);
    shortcuts->Add(new wxButton(this, wxID_HIGHEST + 4, "Connect API"), 0,
                   wxRIGHT, kSpacing);
    shortcuts->Add(new wxButton(this, wxID_HIGHEST + 11, "Disconnect"));
    sizer->Add(shortcuts, 0, wxLEFT | wxRIGHT | wxBOTTOM, kSpacing);
}

ProxyPage::ProxyPage(wxWindow* parent)
    : wxPanel(parent)
{
    auto* sizer = PageSizer(this, "Proxies");
    groupsPane = new wxPanel(this);
    auto* choicesPane = new wxPanel(this);
    groups = new wxRadioBox(groupsPane, wxID_HIGHEST + 12, "Groups",
                             wxDefaultPosition, wxDefaultSize,
                             wxArrayString{"No groups"}, 1, wxRA_SPECIFY_COLS);
    auto* groupsSizer = new wxBoxSizer(wxVERTICAL);
    groupsSizer->Add(groups, 1, wxEXPAND);
    groupsPane->SetSizer(groupsSizer);

    choicesScroll = new wxScrolledWindow(choicesPane, wxID_ANY,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxVSCROLL | wxBORDER_NONE);
    choicesScroll->SetScrollRate(0, 10);
    auto* choicesSizer = new wxBoxSizer(wxVERTICAL);
    choicesSizer->Add(new wxStaticText(choicesScroll, wxID_ANY, "Proxies"), 0,
                      wxBOTTOM, kSpacing);
    choicesScroll->SetSizer(choicesSizer);
    auto* choicesPaneSizer = new wxBoxSizer(wxVERTICAL);
    choicesPaneSizer->Add(choicesScroll, 1, wxEXPAND);
    choicesPane->SetSizer(choicesPaneSizer);

    auto* columns = new wxBoxSizer(wxHORIZONTAL);
    columns->Add(groupsPane, 1, wxEXPAND | wxRIGHT, kSpacing);
    columns->Add(choicesPane, 2, wxEXPAND);
    sizer->Add(columns, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, kSpacing);
}

ConnectionsPage::ConnectionsPage(wxWindow* parent)
    : wxPanel(parent)
{
    auto* sizer = PageSizer(this, "Connections");
    sizer->Add(new wxSearchCtrl(this, wxID_ANY), 0,
               wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, kSpacing);
    table = new wxDataViewListCtrl(this, wxID_ANY);
    AddTableColumn(table, "Target", 220);
    AddTableColumn(table, "Process", 160);
    AddTableColumn(table, "Network", 80);
    AddTableColumn(table, "Rule", 150);
    AddTableColumn(table, "Proxy chain", 180);
    sizer->Add(table, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, kSpacing);
}

LogsPage::LogsPage(wxWindow* parent)
    : wxPanel(parent)
{
    auto* sizer = PageSizer(this, "Logs");
    text = new wxTextCtrl(this, wxID_ANY,
                          "[info] Log view is ready; waiting for Mihomo.\n",
                          wxDefaultPosition, wxDefaultSize,
                          wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    sizer->Add(text, 1, wxEXPAND | wxALL, kSpacing);
}

SettingsPage::SettingsPage(wxWindow* parent)
    : wxPanel(parent)
{
    auto* sizer = PageSizer(this, "Settings");
    auto* connectionForm = new wxFlexGridSizer(2, kSpacing, kSpacing);
    connectionForm->Add(new wxStaticText(this, wxID_ANY, "Mihomo core"));
    auto* coreSizer = new wxBoxSizer(wxHORIZONTAL);
    corePath = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                              wxDefaultPosition, wxDefaultSize,
                              wxTE_PROCESS_ENTER);
    corePath->SetHint("Path to mihomo executable");
    coreSizer->Add(corePath, 1, wxEXPAND | wxRIGHT, kSpacing);
    coreSizer->Add(new wxButton(this, wxID_HIGHEST + 5, "Browse..."));
    connectionForm->Add(coreSizer, 1, wxEXPAND);

    connectionForm->Add(new wxStaticText(this, wxID_ANY, "Mihomo config"));
    auto* configSizer = new wxBoxSizer(wxHORIZONTAL);
    configPath = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxDefaultSize,
                                wxTE_PROCESS_ENTER);
    configPath->SetHint("Path to mihomo YAML config");
    configSizer->Add(configPath, 1, wxEXPAND | wxRIGHT, kSpacing);
    configSizer->Add(new wxButton(this, wxID_HIGHEST + 7, "Browse..."));
    connectionForm->Add(configSizer, 1, wxEXPAND);
    connectionForm->AddGrowableCol(1, 1);
    sizer->Add(connectionForm, 0, wxEXPAND | wxALL, kSpacing);

    auto* displayForm = new wxFlexGridSizer(2, kSpacing, kSpacing);
    logLength = new wxChoice(this, wxID_ANY);
    for (const auto& label : {"10,000 characters", "50,000 characters",
                              "100,000 characters", "500,000 characters",
                              "1,000,000 characters"})
        logLength->Append(label);
    displayForm->Add(new wxStaticText(this, wxID_ANY, "Maximum log length"));
    displayForm->Add(logLength, 1, wxEXPAND);
    displayForm->AddGrowableCol(1, 1);
    sizer->Add(displayForm, 0, wxEXPAND | wxALL, kSpacing);
}

MihomoPage::MihomoPage(wxWindow* parent)
    : wxPanel(parent)
{
    auto* sizer = PageSizer(this, "Mihomo");
    auto* form = new wxFlexGridSizer(2, kSpacing, kSpacing);
    mode = new wxChoice(this, wxID_ANY);
    for (const auto& value : {"Rule", "Global", "Direct"})
        mode->Append(value);
    form->Add(new wxStaticText(this, wxID_ANY, "Mode"));
    form->Add(mode, 1, wxEXPAND);

    mixedPort = new wxTextCtrl(this, wxID_ANY);
    httpPort = new wxTextCtrl(this, wxID_ANY);
    socksPort = new wxTextCtrl(this, wxID_ANY);
    form->Add(new wxStaticText(this, wxID_ANY, "Mixed port"));
    form->Add(mixedPort, 1, wxEXPAND);
    form->Add(new wxStaticText(this, wxID_ANY, "HTTP port"));
    form->Add(httpPort, 1, wxEXPAND);
    form->Add(new wxStaticText(this, wxID_ANY, "SOCKS port"));
    form->Add(socksPort, 1, wxEXPAND);

    controller = new wxTextCtrl(this, wxID_ANY);
    secret = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                            wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    form->Add(new wxStaticText(this, wxID_ANY, "External controller"));
    form->Add(controller, 1, wxEXPAND);
    form->Add(new wxStaticText(this, wxID_ANY, "Secret"));
    form->Add(secret, 1, wxEXPAND);

    logLevel = new wxChoice(this, wxID_ANY);
    for (const auto& value : {"silent", "error", "warning", "info", "debug", "trace"})
        logLevel->Append(value);
    form->Add(new wxStaticText(this, wxID_ANY, "Log level"));
    form->Add(logLevel, 1, wxEXPAND);

    auto* network = new wxBoxSizer(wxHORIZONTAL);
    allowLan = new wxCheckBox(this, wxID_ANY, "Allow LAN connections");
    ipv6 = new wxCheckBox(this, wxID_ANY, "Enable IPv6");
    form->Add(new wxStaticText(this, wxID_ANY, "Network"));
    network->Add(allowLan, 0, wxRIGHT, kSpacing * 2);
    network->Add(ipv6, 0);
    form->Add(network, 1, wxEXPAND);

    auto* tun = new wxBoxSizer(wxHORIZONTAL);
    tunEnable = new wxCheckBox(this, wxID_ANY, "Enable TUN");
    tunStack = new wxChoice(this, wxID_ANY);
    for (const auto& value : {"gvisor", "system", "mixed"})
        tunStack->Append(value);
    form->Add(new wxStaticText(this, wxID_ANY, "TUN"));
    tun->Add(tunEnable, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, kSpacing * 2);
    tun->Add(tunStack, 1, wxEXPAND);
    form->Add(tun, 1, wxEXPAND);

    auto* dns = new wxBoxSizer(wxHORIZONTAL);
    dnsEnable = new wxCheckBox(this, wxID_ANY, "Enable DNS");
    dnsMode = new wxChoice(this, wxID_ANY);
    dnsMode->Append("fake-ip");
    dnsMode->Append("redir-host");
    form->Add(new wxStaticText(this, wxID_ANY, "DNS"));
    dns->Add(dnsEnable, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, kSpacing * 2);
    dns->Add(dnsMode, 1, wxEXPAND);
    form->Add(dns, 1, wxEXPAND);

    nameservers = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, wxDefaultSize,
                                 wxTE_MULTILINE);
    form->Add(new wxStaticText(this, wxID_ANY, "DNS nameservers"));
    form->Add(nameservers, 1, wxEXPAND);
    form->AddGrowableCol(1, 1);
    sizer->Add(form, 1, wxEXPAND | wxALL, kSpacing);
}
