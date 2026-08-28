#include "main_frame.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/srchctrl.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace
{
    constexpr int kSpacing = 8;
    constexpr int kPadding = 12;
    constexpr int kNavigationWidth = 150;

    wxStaticText *AddMetric(wxWindow *parent, wxSizer *sizer,
                            const wxString &title, const wxString &value)
    {
        auto *panel = new wxPanel(parent);
        auto *panelSizer = new wxBoxSizer(wxVERTICAL);
        panelSizer->Add(new wxStaticText(panel, wxID_ANY, title), 0,
                        wxBOTTOM, 4);
        auto *valueText = new wxStaticText(panel, wxID_ANY, value);
        valueText->SetFont(valueText->GetFont().Bold().Scale(1.25));
        panelSizer->Add(valueText, 0);
        panel->SetSizer(panelSizer);
        sizer->Add(panel, 1, wxEXPAND | wxRIGHT, kSpacing);
        return valueText;
    }

    wxPanel *AddPage(wxSimplebook *book, const wxString &title)
    {
        auto *page = new wxPanel(book);
        auto *sizer = new wxBoxSizer(wxVERTICAL);
        auto *heading = new wxStaticText(page, wxID_ANY, title);
        heading->SetFont(heading->GetFont().Bold().Scale(1.35));
        sizer->Add(heading, 0, wxBOTTOM, kSpacing * 2);
        page->SetSizer(sizer);
        book->AddPage(page, title);
        return page;
    }

    void AddTableColumn(wxDataViewListCtrl *table, const wxString &title, int width)
    {
        table->AppendTextColumn(title, wxDATAVIEW_CELL_INERT, width,
                                wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    }
}

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(wxID_HIGHEST + 4, MainFrame::OnConnectApi)
    EVT_BUTTON(wxID_ANY, MainFrame::OnNavigation)
        EVT_CHOICE(wxID_ANY, MainFrame::OnModeChanged)
            wxEND_EVENT_TABLE()

                MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "WxClash", wxDefaultPosition, wxSize(1100, 700),
              wxDEFAULT_FRAME_STYLE)
{
    apiClient_.SetDebugCallback([this](const std::string& message) {
        if (logText_)
            logText_->AppendText(wxString::FromUTF8(message) + "\n\n");
    });

    auto *rootSizer = new wxBoxSizer(wxVERTICAL);
    auto *contentSizer = new wxBoxSizer(wxHORIZONTAL);
    BuildNavigation(contentSizer);
    book_ = new wxSimplebook(this, wxID_ANY);
    contentSizer->Add(book_, 1, wxEXPAND | wxLEFT, kSpacing);
    rootSizer->Add(contentSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, kPadding);

    BuildPages();
    CreateStatusBar(3);
    int statusWidths[] = {-2, -2, -1};
    SetStatusWidths(3, statusWidths);
    SetStatusText("API 127.0.0.1:9090", 0);
    SetStatusText("Down 0 B/s    Up 0 B/s", 1);
    SetStatusText("Not connected", 2);
    SetSizer(rootSizer);
    Centre();
}

void MainFrame::BuildNavigation(wxSizer *parentSizer)
{
    auto *panel = new wxPanel(this, wxID_ANY, wxDefaultPosition,
                              wxSize(kNavigationWidth, -1));
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    AddNavigationButton(panel, sizer, "Overview", PageOverview);
    AddNavigationButton(panel, sizer, "Proxies", PageProxies);
    AddNavigationButton(panel, sizer, "Connections", PageConnections);
    AddNavigationButton(panel, sizer, "Rules", PageRules);
    AddNavigationButton(panel, sizer, "Logs", PageLogs);
    AddNavigationButton(panel, sizer, "Settings", PageSettings);
    sizer->AddStretchSpacer(1);
    panel->SetSizer(sizer);
    parentSizer->Add(panel, 0, wxEXPAND);
}

void MainFrame::AddNavigationButton(wxWindow* parent, wxSizer* sizer,
                                    const wxString& label, PageId page)
{
    auto* button = new wxButton(parent, static_cast<int>(page), label);
    button->SetMinSize(wxSize(kNavigationWidth, 36));
    sizer->Add(button, 0, wxEXPAND | wxBOTTOM, 4);
}

void MainFrame::BuildPages()
{
    auto *overview = AddPage(book_, "Overview");
    auto *overviewSizer = overview->GetSizer();

    auto *modeSizer = new wxBoxSizer(wxHORIZONTAL);
    modeSizer->Add(new wxStaticText(overview, wxID_ANY, "Mode"), 0,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, kSpacing);
    modeChoice_ = new wxChoice(overview, wxID_ANY);
    modeChoice_->Append("Rule");
    modeChoice_->Append("Global");
    modeChoice_->Append("Direct");
    modeChoice_->SetSelection(0);
    modeSizer->Add(modeChoice_, 0, wxALIGN_CENTER_VERTICAL);
    overviewSizer->Add(modeSizer, 0, wxBOTTOM, kSpacing * 2);

    auto *metrics = new wxBoxSizer(wxHORIZONTAL);
    AddMetric(overview, metrics, "Download", "0 B/s");
    AddMetric(overview, metrics, "Upload", "0 B/s");
    AddMetric(overview, metrics, "Active connections", "0");
    AddMetric(overview, metrics, "Mihomo memory", "0 MB");
    overviewSizer->Add(metrics, 0, wxEXPAND | wxBOTTOM, kSpacing * 2);

    auto *chartBox = new wxStaticBoxSizer(wxVERTICAL, overview,
                                          "Traffic (last 120 seconds)");
    chartBox->Add(new wxStaticText(overview, wxID_ANY,
                                   "The traffic chart will be enabled after /traffic is connected."),
                  1, wxALIGN_CENTER | wxALL, kPadding);
    overviewSizer->Add(chartBox, 1, wxEXPAND | wxBOTTOM, kSpacing * 2);

    auto *shortcuts = new wxBoxSizer(wxHORIZONTAL);
    shortcuts->Add(new wxButton(overview, wxID_HIGHEST + 4, "Connect API"), 0,
                   wxRIGHT, kSpacing);
    shortcuts->Add(new wxButton(overview, wxID_ANY, "DNS Query"), 0,
                   wxRIGHT, kSpacing);
    shortcuts->Add(new wxButton(overview, wxID_ANY, "Flush DNS Cache"), 0,
                   wxRIGHT, kSpacing);
    shortcuts->Add(new wxButton(overview, wxID_ANY, "Flush FakeIP Cache"));
    overviewSizer->Add(shortcuts, 0);

    auto *proxies = AddPage(book_, "Proxies");
    auto *proxySizer = proxies->GetSizer();
    auto *proxySplit = new wxBoxSizer(wxHORIZONTAL);
    auto *groups = new wxListBox(proxies, wxID_ANY);
    groups->Append("Selector");
    groups->Append("URL Test");
    groups->Append("Fallback");
    groups->Append("Streaming");
    groups->SetSelection(0);
    proxySplit->Add(groups, 0, wxEXPAND | wxRIGHT, kSpacing);
    auto *proxyTable = new wxDataViewListCtrl(proxies, wxID_ANY);
    AddTableColumn(proxyTable, "Name", 190);
    AddTableColumn(proxyTable, "Type", 110);
    AddTableColumn(proxyTable, "Delay", 90);
    AddTableColumn(proxyTable, "Status", 90);
    proxyTable->AppendItem({"Japan 01", "VMess", "86 ms", "Available"});
    proxyTable->AppendItem({"Hong Kong 02", "Trojan", "112 ms", "Available"});
    proxyTable->AppendItem({"Singapore 01", "Hysteria2", "168 ms", "Available"});
    proxyTable->AppendItem({"United States 03", "VLESS", "Timeout", "Unavailable"});
    proxySplit->Add(proxyTable, 1, wxEXPAND);
    proxySizer->Add(new wxSearchCtrl(proxies, wxID_ANY), 0,
                    wxEXPAND | wxBOTTOM, kSpacing);
    proxySizer->Add(proxySplit, 1, wxEXPAND);

    auto *connections = AddPage(book_, "Connections");
    auto *connectionSizer = connections->GetSizer();
    connectionSizer->Add(new wxSearchCtrl(connections, wxID_ANY), 0,
                         wxEXPAND | wxBOTTOM, kSpacing);
    auto *connectionTable = new wxDataViewListCtrl(connections, wxID_ANY);
    AddTableColumn(connectionTable, "Target", 220);
    AddTableColumn(connectionTable, "Process", 160);
    AddTableColumn(connectionTable, "Network", 80);
    AddTableColumn(connectionTable, "Rule", 150);
    AddTableColumn(connectionTable, "Proxy chain", 180);
    connectionTable->AppendItem({"example.com:443", "browser.exe", "TCP",
                                 "MATCH", "Selector / Japan 01"});
    connectionTable->AppendItem({"dns.google:443", "app.exe", "TCP",
                                 "GEOIP", "Direct"});
    connectionSizer->Add(connectionTable, 1, wxEXPAND);

    auto *rules = AddPage(book_, "Rules");
    auto *ruleSizer = rules->GetSizer();
    ruleSizer->Add(new wxStaticText(rules, wxID_ANY,
                                    "Disabling a rule only affects the current runtime and is lost after restart."),
                   0, wxBOTTOM, kSpacing);
    auto *ruleTable = new wxDataViewListCtrl(rules, wxID_ANY);
    AddTableColumn(ruleTable, "Index", 70);
    AddTableColumn(ruleTable, "Type", 120);
    AddTableColumn(ruleTable, "Match", 300);
    AddTableColumn(ruleTable, "Policy", 150);
    ruleTable->AppendItem({"0", "DOMAIN-SUFFIX", "example.com", "Selector"});
    ruleTable->AppendItem({"1", "GEOIP", "CN", "DIRECT"});
    ruleSizer->Add(ruleTable, 1, wxEXPAND);

    auto *logs = AddPage(book_, "Logs");
    auto *logSizer = logs->GetSizer();
    auto *logText = new wxTextCtrl(logs, wxID_ANY,
                                   "[info] Log view is ready; waiting for Mihomo.\n",
                                   wxDefaultPosition, wxDefaultSize,
                                   wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    logText_ = logText;
    logSizer->Add(logText_, 1, wxEXPAND);

    auto *settings = AddPage(book_, "Settings");
    auto *settingsSizer = settings->GetSizer();
    auto *notebook = new wxNotebook(settings, wxID_ANY);
    auto *connectionPage = new wxPanel(notebook);
    auto *connectionForm = new wxFlexGridSizer(2, kSpacing, kSpacing);
    connectionForm->Add(new wxStaticText(connectionPage, wxID_ANY, "API address"));
    connectionForm->Add(new wxTextCtrl(connectionPage, wxID_ANY, "127.0.0.1:9090"),
                        1, wxEXPAND);
    connectionForm->Add(new wxStaticText(connectionPage, wxID_ANY, "API secret"));
    connectionForm->Add(new wxTextCtrl(connectionPage, wxID_ANY, "",
                                       wxDefaultPosition, wxDefaultSize,
                                       wxTE_PASSWORD),
                        1, wxEXPAND);
    connectionForm->AddGrowableCol(1, 1);
    connectionPage->SetSizer(connectionForm);
    notebook->AddPage(connectionPage, "Connection");

    auto *runtimePage = new wxPanel(notebook);
    auto *runtimeSizer = new wxBoxSizer(wxVERTICAL);
    runtimeSizer->Add(new wxCheckBox(runtimePage, wxID_ANY, "Allow LAN connections"),
                      0, wxBOTTOM, kSpacing);
    runtimeSizer->Add(new wxCheckBox(runtimePage, wxID_ANY, "Enable IPv6"), 0,
                      wxBOTTOM, kSpacing);
    runtimeSizer->Add(new wxStaticText(runtimePage, wxID_ANY,
                                       "Runtime configuration will be editable after /configs is connected."));
    runtimePage->SetSizer(runtimeSizer);
    notebook->AddPage(runtimePage, "Runtime");
    settingsSizer->Add(notebook, 1, wxEXPAND);

    book_->SetSelection(PageOverview);
}

void MainFrame::OnNavigation(wxCommandEvent &event)
{
    const auto page = event.GetId();
    if (page >= 0 && page < PageCount)
        book_->SetSelection(page);
}

void MainFrame::OnModeChanged(wxCommandEvent &event)
{
    if (event.GetEventObject() != modeChoice_)
        return;

    SetStatusText("Mode: " + modeChoice_->GetStringSelection(), 2);
}

void MainFrame::OnConnectApi(wxCommandEvent&)
{
    const auto response = apiClient_.GetVersion();
    if (!response.ok)
    {
        SetStatusText("API error: " + wxString::FromUTF8(response.error), 2);
        return;
    }
    SetStatusText("API connected", 2);
}
