#include "main_frame.h"

#include <nlohmann/json.hpp>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/ffile.h>
#include <wx/dataview.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/filename.h>
#include <wx/radiobox.h>
#include <wx/radiobut.h>
#include <wx/scrolwin.h>
#include <wx/srchctrl.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tokenzr.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>

namespace
{
    using Json = nlohmann::json;
    constexpr int kSpacing = 8;
    constexpr int kPadding = 12;
    constexpr int kNavigationWidth = 150;

    std::string FormatBytes(std::uint64_t bytes)
    {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        double value = static_cast<double>(bytes);
        std::size_t unit = 0;
        while (value >= 1024.0 && unit < 4)
        {
            value /= 1024.0;
            ++unit;
        }
        std::ostringstream output;
        if (unit == 0)
            output << bytes;
        else
            output.setf(std::ios::fixed), output.precision(value < 10 ? 1 : 0),
                output << value;
        output << ' ' << units[unit];
        return output.str();
    }

    std::string JsonString(const Json* object, const char* key,
                           const std::string& fallback = {})
    {
        if (!object || !object->is_object() || !object->contains(key) ||
            !(*object)[key].is_string())
            return fallback;
        return (*object)[key].get<std::string>();
    }

    std::uint64_t JsonUint64(const Json* object, const char* key)
    {
        if (!object || !object->is_object() || !object->contains(key))
            return 0;
        try
        {
            return (*object)[key].get<std::uint64_t>();
        }
        catch (const Json::type_error&)
        {
            return 0;
        }
    }

    wxString SettingsPath()
    {
        const auto directory = wxString::FromUTF8(MihomoSidecar::DefaultDataPath());
        wxFileName::Mkdir(directory, 0700, wxPATH_MKDIR_FULL);
        return wxFileName(directory, "settings.conf").GetFullPath();
    }

    std::string MihomoOverridesPath(const std::string& dataPath)
    {
        return wxFileName(wxString::FromUTF8(dataPath), "mihomo-settings.yaml")
            .GetFullPath().ToStdString();
    }

    std::string MihomoRuntimePath(const std::string& dataPath)
    {
        return wxFileName(wxString::FromUTF8(dataPath), "runtime.yaml")
            .GetFullPath().ToStdString();
    }

    void LoadSettings(std::string& corePath, std::string& configPath,
                      std::size_t& maxLogLength)
    {
        wxFFile file(SettingsPath(), "r");
        if (!file.IsOpened())
            return;

        wxString contents;
        if (!file.ReadAll(&contents))
            return;

        wxStringTokenizer lines(contents, "\n", wxTOKEN_RET_EMPTY_ALL);
        while (lines.HasMoreTokens())
        {
            const auto line = lines.GetNextToken();
            if (line.StartsWith("core_path="))
                corePath = line.Mid(10).ToStdString();
            else if (line.StartsWith("config_path="))
                configPath = line.Mid(12).ToStdString();
            else if (line.StartsWith("max_log_length="))
            {
                unsigned long value = 0;
                if (line.Mid(15).ToULong(&value) &&
                    (value == 10000 || value == 50000 || value == 100000 ||
                     value == 500000 || value == 1000000))
                    maxLogLength = static_cast<std::size_t>(value);
            }
        }
    }

    void SaveSettings(const std::string& corePath, const std::string& configPath,
                      std::size_t maxLogLength)
    {
        wxFFile file(SettingsPath(), "w");
        if (!file.IsOpened())
        {
            std::cerr << "[WxClash] Unable to open settings file for writing"
                      << std::endl;
            return;
        }

        const wxString contents = "core_path=" + wxString::FromUTF8(corePath) +
                                  "\nconfig_path=" + wxString::FromUTF8(configPath) +
                                  "\nmax_log_length=" + wxString::Format("%zu", maxLogLength) +
                                  "\n";
        if (!file.Write(contents) || !file.Close())
            std::cerr << "[WxClash] Unable to write settings file" << std::endl;
    }

    wxPanel *AddPage(wxSimplebook *book, const wxString &title)
    {
        auto *page = new wxPanel(book);
        auto *sizer = new wxBoxSizer(wxVERTICAL);
        auto *heading = new wxStaticText(page, wxID_ANY, title);
        heading->SetFont(heading->GetFont().Bold().Scale(1.35));
        sizer->Add(heading, 0, wxALL, kSpacing);
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
    EVT_CLOSE(MainFrame::OnClose)
    EVT_TIMER(wxID_HIGHEST + 8, MainFrame::OnSidecarOutput)
    EVT_TIMER(wxID_HIGHEST + 10, MainFrame::OnMonitorTimer)
    EVT_BUTTON(wxID_HIGHEST + 4, MainFrame::OnConnectApi)
    EVT_BUTTON(wxID_HIGHEST + 11, MainFrame::OnDisconnectApi)
    EVT_RADIOBOX(wxID_HIGHEST + 12, MainFrame::OnProxyGroupSelected)
    EVT_RADIOBUTTON(wxID_HIGHEST + 13, MainFrame::OnProxySelected)
    EVT_BUTTON(wxID_HIGHEST + 5, MainFrame::OnBrowseCore)
    EVT_BUTTON(wxID_HIGHEST + 7, MainFrame::OnBrowseConfig)
    EVT_BUTTON(wxID_ANY, MainFrame::OnNavigation)
        EVT_CHOICE(wxID_ANY, MainFrame::OnModeChanged)
            wxEND_EVENT_TABLE()

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "WxClash", wxDefaultPosition, wxSize(1100, 700),
      wxDEFAULT_FRAME_STYLE),
      sidecarOutputTimer_(this, wxID_HIGHEST + 8),
      monitorTimer_(this, wxID_HIGHEST + 10)
{
    LoadSettings(corePath_, configPath_, maxLogLength_);
    dataPath_ = MihomoSidecar::DefaultDataPath();
    if (configPath_.empty())
        configPath_ = wxFileName(wxString::FromUTF8(dataPath_), "config.yaml")
                          .GetFullPath().ToStdString();
    std::string configError;
    if (!mihomoConfig_.Load(configPath_, configError))
        std::cerr << "[WxClash] " << configError << std::endl;
    if (!mihomoConfig_.LoadOverrides(MihomoOverridesPath(dataPath_), configError))
        std::cerr << "[WxClash] " << configError << std::endl;

    mihomoSidecar_.SetOutputCallback([this](const std::string& message) {
        AppendLog(wxString::FromUTF8(message) + "\n");
    });
    mihomoSidecar_.SetTerminationCallback([this] {
        if (closing_)
            Destroy();
        else
        {
            if (overviewStatus_)
                overviewStatus_->SetLabel("Not connected");
            if (overviewVersion_)
                overviewVersion_->SetLabel("Mihomo version: -");
            if (overviewConnections_)
                overviewConnections_->SetLabel("Connections: -");
            if (overviewTraffic_)
                overviewTraffic_->SetLabel("Traffic: -");
        }
    });

    auto *rootSizer = new wxBoxSizer(wxVERTICAL);
    auto *contentSizer = new wxBoxSizer(wxHORIZONTAL);
    BuildNavigation(contentSizer);
    book_ = new wxSimplebook(this, wxID_ANY);
    contentSizer->Add(book_, 1, wxEXPAND | wxLEFT, kSpacing);
    rootSizer->Add(contentSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, kPadding);

    BuildPages();
    SetSizer(rootSizer);
    Centre();
}

MainFrame::~MainFrame()
{
    sidecarOutputTimer_.Stop();
    monitorTimer_.Stop();
    if (mihomoModeChoice_)
    {
        std::string ignoredError;
        SaveMihomoSettings(ignoredError);
    }
    if (configPathText_)
        configPath_ = configPathText_->GetValue().ToStdString();
    SaveSettings(corePath_, configPath_, maxLogLength_);
}

void MainFrame::BuildNavigation(wxSizer *parentSizer)
{
    auto *panel = new wxPanel(this, wxID_ANY, wxDefaultPosition,
                              wxSize(kNavigationWidth, -1));
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    AddNavigationButton(panel, sizer, "Overview", PageOverview);
    AddNavigationButton(panel, sizer, "Proxies", PageProxies);
    AddNavigationButton(panel, sizer, "Connections", PageConnections);
    AddNavigationButton(panel, sizer, "Logs", PageLogs);
    AddNavigationButton(panel, sizer, "Settings", PageSettings);
    AddNavigationButton(panel, sizer, "Mihomo", PageMihomo);
    sizer->AddStretchSpacer(1);
    panel->SetSizer(sizer);
    parentSizer->Add(panel, 0, wxEXPAND);
}

void MainFrame::AddNavigationButton(wxWindow* parent, wxSizer* sizer,
                                    const wxString& label, PageId page)
{
    auto* button = new wxButton(parent, static_cast<int>(page), label);
    button->SetMinSize(wxSize(kNavigationWidth, 36));
    sizer->Add(button, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, kSpacing);
}

void MainFrame::BuildPages()
{
    auto *overview = AddPage(book_, "Overview");
    auto *overviewSizer = overview->GetSizer();

    auto *overviewInfo = new wxFlexGridSizer(2, kSpacing, kSpacing);
    overviewStatus_ = new wxStaticText(overview, wxID_ANY, "Not connected");
    overviewVersion_ = new wxStaticText(overview, wxID_ANY, "Mihomo version: -");
    overviewConnections_ = new wxStaticText(overview, wxID_ANY, "Connections: -");
    overviewTraffic_ = new wxStaticText(overview, wxID_ANY, "Traffic: -");
    overviewInfo->Add(new wxStaticText(overview, wxID_ANY, "Status"));
    overviewInfo->Add(overviewStatus_, 1, wxEXPAND);
    overviewInfo->Add(new wxStaticText(overview, wxID_ANY, "Version"));
    overviewInfo->Add(overviewVersion_, 1, wxEXPAND);
    overviewInfo->Add(new wxStaticText(overview, wxID_ANY, "Connections"));
    overviewInfo->Add(overviewConnections_, 1, wxEXPAND);
    overviewInfo->Add(new wxStaticText(overview, wxID_ANY, "Traffic"));
    overviewInfo->Add(overviewTraffic_, 1, wxEXPAND);
    overviewInfo->AddGrowableCol(1, 1);
    overviewSizer->Add(overviewInfo, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                       kSpacing);

    auto *shortcuts = new wxBoxSizer(wxHORIZONTAL);
    shortcuts->Add(new wxButton(overview, wxID_HIGHEST + 4, "Connect API"), 0,
                   wxRIGHT, kSpacing);
    shortcuts->Add(new wxButton(overview, wxID_HIGHEST + 11, "Disconnect"), 0);
    overviewSizer->Add(shortcuts, 0, wxLEFT | wxRIGHT | wxBOTTOM, kSpacing);

    auto *proxies = AddPage(book_, "Proxies");
    auto *proxySizer = proxies->GetSizer();
    proxyGroupsPane_ = new wxPanel(proxies);
    proxyChoicesPane_ = new wxPanel(proxies);
    proxyGroups_ = new wxRadioBox(proxyGroupsPane_, wxID_HIGHEST + 12,
                                  "Groups", wxDefaultPosition, wxDefaultSize,
                                  wxArrayString{"No groups"}, 1,
                                  wxRA_SPECIFY_COLS);
    auto *groupPaneSizer = new wxBoxSizer(wxVERTICAL);
    groupPaneSizer->Add(proxyGroups_, 1, wxEXPAND);
    proxyGroupsPane_->SetSizer(groupPaneSizer);
    auto *choicePaneSizer = new wxBoxSizer(wxVERTICAL);
    proxyChoicesScroll_ = new wxScrolledWindow(proxyChoicesPane_, wxID_ANY,
                                               wxDefaultPosition, wxDefaultSize,
                                               wxVSCROLL | wxBORDER_NONE);
    proxyChoicesScroll_->SetScrollRate(0, 10);
    auto *choiceScrollSizer = new wxBoxSizer(wxVERTICAL);
    choiceScrollSizer->Add(new wxStaticText(proxyChoicesScroll_, wxID_ANY, "Proxies"),
                            0, wxBOTTOM, kSpacing);
    proxyChoicesScroll_->SetSizer(choiceScrollSizer);
    choicePaneSizer->Add(proxyChoicesScroll_, 1, wxEXPAND);
    proxyChoicesPane_->SetSizer(choicePaneSizer);
    auto *proxyColumns = new wxBoxSizer(wxHORIZONTAL);
    proxyColumns->Add(proxyGroupsPane_, 1, wxEXPAND | wxRIGHT, kSpacing);
    proxyColumns->Add(proxyChoicesPane_, 2, wxEXPAND);
    proxySizer->Add(proxyColumns, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                    kSpacing);

    auto *connections = AddPage(book_, "Connections");
    auto *connectionSizer = connections->GetSizer();
    connectionSizer->Add(new wxSearchCtrl(connections, wxID_ANY), 0,
                         wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, kSpacing);
    connectionTable_ = new wxDataViewListCtrl(connections, wxID_ANY);
    AddTableColumn(connectionTable_, "Target", 220);
    AddTableColumn(connectionTable_, "Process", 160);
    AddTableColumn(connectionTable_, "Network", 80);
    AddTableColumn(connectionTable_, "Rule", 150);
    AddTableColumn(connectionTable_, "Proxy chain", 180);
    connectionSizer->Add(connectionTable_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                         kSpacing);

    auto *logs = AddPage(book_, "Logs");
    auto *logSizer = logs->GetSizer();
    auto *logText = new wxTextCtrl(logs, wxID_ANY,
                                   "[info] Log view is ready; waiting for Mihomo.\n",
                                   wxDefaultPosition, wxDefaultSize,
                                   wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    logText_ = logText;
    logSizer->Add(logText_, 1, wxEXPAND | wxALL, kSpacing);

    auto *settings = AddPage(book_, "Settings");
    auto *settingsSizer = settings->GetSizer();
    auto *connectionPage = settings;
    auto *connectionForm = new wxFlexGridSizer(2, kSpacing, kSpacing);
    connectionForm->Add(new wxStaticText(connectionPage, wxID_ANY, "Mihomo core"));
    auto *corePathSizer = new wxBoxSizer(wxHORIZONTAL);
    corePathText_ = new wxTextCtrl(connectionPage, wxID_ANY,
                                    wxEmptyString, wxDefaultPosition,
                                    wxDefaultSize, wxTE_PROCESS_ENTER);
    corePathText_->SetHint("Path to mihomo executable");
    corePathSizer->Add(corePathText_, 1, wxEXPAND | wxRIGHT, kSpacing);
    corePathSizer->Add(new wxButton(connectionPage, wxID_HIGHEST + 5, "Browse..."),
                       0, wxEXPAND);
    connectionForm->Add(corePathSizer, 1, wxEXPAND);
    connectionForm->Add(new wxStaticText(connectionPage, wxID_ANY, "Mihomo config"));
    auto *configPathSizer = new wxBoxSizer(wxHORIZONTAL);
    configPathText_ = new wxTextCtrl(connectionPage, wxID_ANY,
                                     wxEmptyString, wxDefaultPosition,
                                     wxDefaultSize, wxTE_PROCESS_ENTER);
    configPathText_->SetHint("Path to mihomo YAML config");
    configPathSizer->Add(configPathText_, 1, wxEXPAND | wxRIGHT, kSpacing);
    configPathSizer->Add(new wxButton(connectionPage, wxID_HIGHEST + 7, "Browse..."),
                         0, wxEXPAND);
    connectionForm->Add(configPathSizer, 1, wxEXPAND);
    connectionForm->AddGrowableCol(1, 1);
    settingsSizer->Add(connectionForm, 0, wxEXPAND | wxALL, kSpacing);

    auto *mihomoPage = AddPage(book_, "Mihomo");
    auto *mihomoPageSizer = mihomoPage->GetSizer();
    auto *mihomoForm = new wxFlexGridSizer(2, kSpacing, kSpacing);
    mihomoModeChoice_ = new wxChoice(mihomoPage, wxID_ANY);
    mihomoModeChoice_->Append("Rule");
    mihomoModeChoice_->Append("Global");
    mihomoModeChoice_->Append("Direct");
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "Mode"));
    mihomoForm->Add(mihomoModeChoice_, 1, wxEXPAND);

    mixedPortText_ = new wxTextCtrl(mihomoPage, wxID_ANY);
    httpPortText_ = new wxTextCtrl(mihomoPage, wxID_ANY);
    socksPortText_ = new wxTextCtrl(mihomoPage, wxID_ANY);
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "Mixed port"));
    mihomoForm->Add(mixedPortText_, 1, wxEXPAND);
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "HTTP port"));
    mihomoForm->Add(httpPortText_, 1, wxEXPAND);
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "SOCKS port"));
    mihomoForm->Add(socksPortText_, 1, wxEXPAND);

    controllerText_ = new wxTextCtrl(mihomoPage, wxID_ANY);
    secretText_ = new wxTextCtrl(mihomoPage, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "External controller"));
    mihomoForm->Add(controllerText_, 1, wxEXPAND);
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "Secret"));
    mihomoForm->Add(secretText_, 1, wxEXPAND);

    mihomoLogLevelChoice_ = new wxChoice(mihomoPage, wxID_ANY);
    for (const auto& level : {"silent", "error", "warning", "info", "debug", "trace"})
        mihomoLogLevelChoice_->Append(level);
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "Log level"));
    mihomoForm->Add(mihomoLogLevelChoice_, 1, wxEXPAND);

    allowLanCheck_ = new wxCheckBox(mihomoPage, wxID_ANY, "Allow LAN connections");
    ipv6Check_ = new wxCheckBox(mihomoPage, wxID_ANY, "Enable IPv6");
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "Network"));
    auto *networkChecks = new wxBoxSizer(wxHORIZONTAL);
    networkChecks->Add(allowLanCheck_, 0, wxRIGHT, kSpacing * 2);
    networkChecks->Add(ipv6Check_, 0);
    mihomoForm->Add(networkChecks, 1, wxEXPAND);

    tunEnableCheck_ = new wxCheckBox(mihomoPage, wxID_ANY, "Enable TUN");
    tunStackChoice_ = new wxChoice(mihomoPage, wxID_ANY);
    for (const auto& stack : {"gvisor", "system", "mixed"})
        tunStackChoice_->Append(stack);
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "TUN"));
    auto *tunControls = new wxBoxSizer(wxHORIZONTAL);
    tunControls->Add(tunEnableCheck_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, kSpacing * 2);
    tunControls->Add(tunStackChoice_, 1, wxEXPAND);
    mihomoForm->Add(tunControls, 1, wxEXPAND);

    dnsEnableCheck_ = new wxCheckBox(mihomoPage, wxID_ANY, "Enable DNS");
    dnsModeChoice_ = new wxChoice(mihomoPage, wxID_ANY);
    dnsModeChoice_->Append("fake-ip");
    dnsModeChoice_->Append("redir-host");
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "DNS"));
    auto *dnsControls = new wxBoxSizer(wxHORIZONTAL);
    dnsControls->Add(dnsEnableCheck_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, kSpacing * 2);
    dnsControls->Add(dnsModeChoice_, 1, wxEXPAND);
    mihomoForm->Add(dnsControls, 1, wxEXPAND);

    nameserverText_ = new wxTextCtrl(mihomoPage, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize,
                                     wxTE_MULTILINE);
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, "DNS nameservers"));
    mihomoForm->Add(nameserverText_, 1, wxEXPAND);
    mihomoForm->AddGrowableCol(1, 1);
    mihomoPageSizer->Add(mihomoForm, 1, wxEXPAND | wxALL, kSpacing);

    mihomoPage->SetSizer(mihomoPageSizer);
    auto *displayForm = new wxFlexGridSizer(2, kSpacing, kSpacing);
    logLengthChoice_ = new wxChoice(settings, wxID_ANY);
    logLengthChoice_->Append("10,000 characters");
    logLengthChoice_->Append("50,000 characters");
    logLengthChoice_->Append("100,000 characters");
    logLengthChoice_->Append("500,000 characters");
    logLengthChoice_->Append("1,000,000 characters");
    displayForm->Add(new wxStaticText(settings, wxID_ANY, "Maximum log length"));
    displayForm->Add(logLengthChoice_, 1, wxEXPAND);
    displayForm->AddGrowableCol(1, 1);
    settingsSizer->Add(displayForm, 0, wxEXPAND | wxALL, kSpacing);

    UpdateMihomoControls();
    const std::size_t logLengthValues[] = {10000, 50000, 100000, 500000, 1000000};
    for (unsigned int index = 0; index < 5; ++index)
        if (logLengthValues[index] == maxLogLength_)
            logLengthChoice_->SetSelection(index);
    if (logLengthChoice_->GetSelection() == wxNOT_FOUND)
        logLengthChoice_->SetSelection(2);

    if (!corePath_.empty())
        corePathText_->SetValue(wxString::FromUTF8(corePath_));
    if (!configPath_.empty())
        configPathText_->SetValue(wxString::FromUTF8(configPath_));

    book_->SetSelection(PageOverview);
}

void MainFrame::OnNavigation(wxCommandEvent &event)
{
    const auto page = event.GetId();
    if (page >= 0 && page < PageCount)
    {
        book_->SetSelection(page);
        if (page == PageProxies)
            RefreshProxies();
    }
}

void MainFrame::OnModeChanged(wxCommandEvent &event)
{
    if (event.GetEventObject() == logLengthChoice_)
    {
        const std::size_t logLengthValues[] = {10000, 50000, 100000, 500000, 1000000};
        if (logLengthChoice_->GetSelection() >= 0)
            maxLogLength_ = logLengthValues[logLengthChoice_->GetSelection()];
        AppendLog(wxString::Format("[info] Display settings updated; log limit: %zu characters\n",
                                   maxLogLength_));
        return;
    }
}

void MainFrame::OnSidecarOutput(wxTimerEvent&)
{
    if (mihomoSidecar_.IsRunning())
        mihomoSidecar_.PollOutput();
}

void MainFrame::OnMonitorTimer(wxTimerEvent&)
{
    if (!apiConnected_)
        return;
    const auto page = book_ ? book_->GetSelection() : wxNOT_FOUND;
    if (page == PageOverview || page == PageConnections)
        RefreshCoreData();
}

void MainFrame::AppendLog(const wxString& message)
{
    if (!logText_)
        return;
    logText_->AppendText(message);
    const auto length = static_cast<std::size_t>(logText_->GetLastPosition());
    if (length > maxLogLength_)
        logText_->Remove(0, static_cast<long>(length - maxLogLength_));
}

void MainFrame::RefreshCoreData()
{
    if (!apiConnected_)
        return;

    const auto connectionsResponse = apiClient_.GetConnections();
    if (!connectionsResponse.ok)
    {
        AppendLog("[error] Monitor error (/connections): " +
                  wxString::FromUTF8(connectionsResponse.error) + "\n");
        return;
    }

    Json connectionsRoot;
    try
    {
        connectionsRoot = Json::parse(connectionsResponse.body);
    }
    catch (const Json::parse_error& exception)
    {
        AppendLog("[error] Invalid /connections JSON: " +
                  wxString::FromUTF8(exception.what()) + "\n");
        return;
    }
    if (!connectionsRoot.is_object())
    {
        AppendLog("[error] Invalid /connections JSON: expected an object\n");
        return;
    }
    {
        const auto connections = connectionsRoot.find("connections");
        const auto connectionCount = connections != connectionsRoot.end() &&
                                              connections->is_array()
                                          ? connections->size()
                                          : 0;
        if (overviewConnections_)
            overviewConnections_->SetLabel(
                wxString::Format("Connections: %zu", connectionCount));

        if (connectionTable_)
        {
            connectionTable_->DeleteAllItems();
            if (connections != connectionsRoot.end() && connections->is_array())
            {
                for (const auto& connection : *connections)
                {
                    const auto metadata = connection.find("metadata");
                    const auto metadataObject = metadata != connection.end() ? &(*metadata) : nullptr;
                    std::string target = JsonString(metadataObject, "host");
                    if (target.empty())
                        target = JsonString(metadataObject, "destinationIP");
                    const auto port = JsonString(metadataObject, "destinationPort");
                    if (!port.empty())
                        target += ":" + port;

                    std::string chains;
                    const auto chain = connection.find("chains");
                    if (chain != connection.end() && chain->is_array())
                    {
                        for (std::size_t index = 0; index < chain->size(); ++index)
                        {
                            if (index != 0)
                                chains += " / ";
                            if ((*chain)[index].is_string())
                                chains += (*chain)[index].get<std::string>();
                        }
                    }
                    connectionTable_->AppendItem(
                        {target,
                         JsonString(metadataObject, "process"),
                         JsonString(metadataObject, "network"),
                         JsonString(&connection, "rule"),
                         chains});
                }
            }
        }
    }

    const auto trafficResponse = apiClient_.GetTraffic();
    if (!trafficResponse.ok)
    {
        AppendLog("[error] Monitor error (/traffic): " +
                  wxString::FromUTF8(trafficResponse.error) + "\n");
    }
    else
    {
        try
        {
            const auto traffic = Json::parse(trafficResponse.body);
            if (!traffic.is_object())
            {
                AppendLog("[error] Invalid /traffic JSON: expected an object\n");
                return;
            }
            if (overviewTraffic_)
                overviewTraffic_->SetLabel(
                    "Download: " +
                    wxString::FromUTF8(FormatBytes(JsonUint64(&traffic, "down"))) +
                    "/s  Upload: " +
                    wxString::FromUTF8(FormatBytes(JsonUint64(&traffic, "up"))) +
                    "/s");
        }
        catch (const Json::parse_error& exception)
        {
            AppendLog("[error] Invalid /traffic JSON: " +
                      wxString::FromUTF8(exception.what()) + "\n");
        }
    }

}

void MainFrame::RefreshProxies()
{
    if (!apiConnected_)
    {
        AppendLog("[warning] Proxy refresh skipped: API is not connected\n");
        return;
    }
    const auto response = apiClient_.GetProxies();
    if (!response.ok)
    {
        AppendLog("[error] Monitor error (/proxies): " +
                  wxString::FromUTF8(response.error) + "\n");
        return;
    }
    Json root;
    try
    {
        root = Json::parse(response.body);
    }
    catch (const Json::parse_error& exception)
    {
        AppendLog("[error] Invalid /proxies JSON: " +
                  wxString::FromUTF8(exception.what()) + "\n");
        return;
    }
    if (!root.is_object())
    {
        AppendLog("[error] Invalid /proxies JSON: expected an object\n");
        return;
    }
    proxyData_ = root;
    proxyGroupMembers_.clear();
    proxyCurrentSelection_.clear();
    proxyGroupNames_.clear();
    updatingProxyGroups_ = true;
    const auto proxies = root.find("proxies");
    if (proxies == root.end() || !proxies->is_object())
    {
        AppendLog("[error] Invalid /proxies JSON: missing object field 'proxies'\n");
        return;
    }
    if (proxies->empty())
        AppendLog("[warning] Proxy refresh returned no proxy entries\n");
    else
    {
        for (const auto& entry : proxies->items())
        {
            const auto& name = entry.key();
            const auto& proxy = entry.value();
            const auto type = JsonString(&proxy, "type");
            const auto isGroup = type == "Selector" || type == "URLTest" ||
                                 type == "Fallback" || type == "LoadBalance" ||
                                 type == "Relay";
            if (!isGroup)
                continue;
            const auto all = proxy.find("all");
            if (all != proxy.end() && all->is_array())
                for (const auto& member : *all)
                    if (member.is_string())
                        proxyGroupMembers_[name].push_back(member.get<std::string>());
            // Some controller versions expose the group members as `proxies`
            // instead of `all`; accept both response shapes.
            if (proxyGroupMembers_[name].empty())
            {
                const auto members = proxy.find("proxies");
                if (members != proxy.end() && members->is_array())
                    for (const auto& member : *members)
                        if (member.is_string())
                            proxyGroupMembers_[name].push_back(member.get<std::string>());
            }
            proxyCurrentSelection_[name] = JsonString(&proxy, "now");
            proxyGroupNames_.push_back(name);
        }
    }
    auto *groupSizer = proxyGroupsPane_ ? proxyGroupsPane_->GetSizer() : nullptr;
    if (groupSizer)
    {
        groupSizer->Detach(proxyGroups_);
        proxyGroups_->Destroy();
        wxArrayString labels;
        for (const auto& name : proxyGroupNames_)
            labels.Add(wxString::FromUTF8(name));
        if (labels.empty())
            labels.Add("No groups");
        proxyGroups_ = new wxRadioBox(proxyGroupsPane_, wxID_HIGHEST + 12,
                                      "Groups", wxDefaultPosition, wxDefaultSize,
                                      labels, 1, wxRA_SPECIFY_COLS);
        groupSizer->Add(proxyGroups_, 1, wxEXPAND);
        proxyGroupsPane_->Layout();
    }
    if (!proxyGroupNames_.empty())
    {
        int selection = proxyGroups_->FindString(wxString::FromUTF8(selectedProxyGroup_));
        if (selection == wxNOT_FOUND)
            selection = 0;
        proxyGroups_->SetSelection(selection);
        selectedProxyGroup_ = proxyGroupNames_[static_cast<std::size_t>(selection)];
    }
    else
        selectedProxyGroup_.clear();
    updatingProxyGroups_ = false;
    PopulateProxyChoices();
}

void MainFrame::OnProxyGroupSelected(wxCommandEvent& event)
{
    if (event.GetEventObject() != proxyGroups_ || !proxyGroups_)
        return;
    if (updatingProxyGroups_)
        return;
    const int selection = proxyGroups_->GetSelection();
    if (selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= proxyGroupNames_.size())
        return;
    selectedProxyGroup_ = proxyGroupNames_[static_cast<std::size_t>(selection)];
    RefreshProxyGroup();
}

void MainFrame::RefreshProxyGroup()
{
    if (selectedProxyGroup_.empty())
    {
        AppendLog("[error] Proxy group refresh failed: group name is empty\n");
        return;
    }

    const auto response = apiClient_.GetProxy(selectedProxyGroup_);
    if (!response.ok)
    {
        AppendLog("[error] Proxy group refresh failed (" +
                  wxString::FromUTF8(selectedProxyGroup_) + "): " +
                  wxString::FromUTF8(response.error) + "\n");
        return;
    }

    Json group;
    try
    {
        group = Json::parse(response.body);
    }
    catch (const Json::parse_error& exception)
    {
        AppendLog("[error] Invalid proxy group JSON (" +
                  wxString::FromUTF8(selectedProxyGroup_) + "): " +
                  wxString::FromUTF8(exception.what()) + "\n");
        return;
    }
    if (!group.is_object())
    {
        AppendLog("[error] Invalid proxy group JSON: expected an object\n");
        return;
    }

    std::vector<std::string> members;
    const auto all = group.find("all");
    if (all != group.end() && all->is_array())
        for (const auto& member : *all)
            if (member.is_string())
                members.push_back(member.get<std::string>());
    if (members.empty())
    {
        const auto proxies = group.find("proxies");
        if (proxies != group.end() && proxies->is_array())
            for (const auto& member : *proxies)
                if (member.is_string())
                    members.push_back(member.get<std::string>());
    }
    if (members.empty())
    {
        AppendLog("[error] Proxy group has no members in API response: " +
                  wxString::FromUTF8(selectedProxyGroup_) + "\n");
        return;
    }

    proxyGroupMembers_[selectedProxyGroup_] = std::move(members);
    proxyCurrentSelection_[selectedProxyGroup_] = JsonString(&group, "now");
    PopulateProxyChoices();
}

void MainFrame::PopulateProxyChoices()
{
    if (!proxyChoicesScroll_)
    {
        return;
    }
    updatingProxyTable_ = true;
    proxyChoiceNames_.clear();
    const auto proxies = proxyData_.find("proxies");
    if (proxies == proxyData_.end() || !proxies->is_object())
    {
        AppendLog("[error] Proxy refresh failed: cached response has no proxy object\n");
        updatingProxyTable_ = false;
        return;
    }
    const auto members = proxyGroupMembers_.find(selectedProxyGroup_);
    if (members == proxyGroupMembers_.end())
    {
        AppendLog("[warning] Proxy group has no member list: " +
                  wxString::FromUTF8(selectedProxyGroup_) + "\n");
        updatingProxyTable_ = false;
        return;
    }
    const auto current = proxyCurrentSelection_.find(selectedProxyGroup_);
    for (const auto& name : members->second)
    {
        const auto proxy = proxies->find(name);
        if (proxy == proxies->end())
        {
            // Keep the member visible even when this controller omits its
            // detail object from the top-level /proxies response.
            proxyChoiceNames_.push_back(name);
            continue;
        }
        proxyChoiceNames_.push_back(name);
    }
    auto *choiceSizer = proxyChoicesScroll_->GetSizer();
    proxyChoiceButtons_.clear();
    choiceSizer->Clear(true);
    choiceSizer->Add(new wxStaticText(proxyChoicesScroll_, wxID_ANY, "Proxies"),
                     0, wxBOTTOM, kSpacing);
    for (const auto& name : proxyChoiceNames_)
    {
        const auto proxy = proxies->find(name);
        std::string label = name;
        if (proxy != proxies->end())
            label += "  [" + JsonString(&(*proxy), "type", "Proxy") + "]";
        if (current != proxyCurrentSelection_.end() && current->second == name)
            label += "  (selected)";
        auto *button = new wxRadioButton(proxyChoicesScroll_, wxID_HIGHEST + 13,
                                         wxString::FromUTF8(label),
                                         wxDefaultPosition, wxDefaultSize,
                                         proxyChoiceButtons_.empty() ? wxRB_GROUP : 0);
        proxyChoiceButtons_.push_back(button);
        choiceSizer->Add(button, 0, wxEXPAND | wxBOTTOM, 4);
    }
    const auto currentName = proxyCurrentSelection_.find(selectedProxyGroup_);
    if (currentName != proxyCurrentSelection_.end())
    {
        const auto selectedIndex = std::find(proxyChoiceNames_.begin(),
                                             proxyChoiceNames_.end(),
                                             currentName->second);
        if (selectedIndex != proxyChoiceNames_.end())
            proxyChoiceButtons_[static_cast<std::size_t>(std::distance(
                proxyChoiceNames_.begin(), selectedIndex))]->SetValue(true);
    }
    if (proxyChoiceButtons_.empty())
        choiceSizer->Add(new wxStaticText(proxyChoicesScroll_, wxID_ANY, "No proxies"),
                         0, wxEXPAND);
    proxyChoicesScroll_->FitInside();
    proxyChoicesScroll_->Layout();
    updatingProxyTable_ = false;
}

void MainFrame::OnProxySelected(wxCommandEvent& event)
{
    if (updatingProxyTable_ || selectedProxyGroup_.empty())
        return;
    auto *button = dynamic_cast<wxRadioButton*>(event.GetEventObject());
    const auto it = std::find(proxyChoiceButtons_.begin(),
                              proxyChoiceButtons_.end(), button);
    if (it == proxyChoiceButtons_.end())
        return;
    SelectProxy(proxyChoiceNames_[static_cast<std::size_t>(
        std::distance(proxyChoiceButtons_.begin(), it))]);
}

void MainFrame::SelectProxy(const std::string& proxyName)
{
    if (!proxyChoicesScroll_ || selectedProxyGroup_.empty())
    {
        AppendLog("[error] Proxy selection failed: no proxy group is selected\n");
        return;
    }
    if (proxyName.empty())
    {
        AppendLog("[error] Proxy selection failed: selected proxy name is empty\n");
        return;
    }
    const auto response = apiClient_.SelectProxy(selectedProxyGroup_, proxyName);
    if (!response.ok)
    {
        AppendLog("[error] Proxy selection failed: " + wxString::FromUTF8(response.error) + "\n");
        return;
    }
    proxyCurrentSelection_[selectedProxyGroup_] = proxyName;
    PopulateProxyChoices();
    AppendLog("[info] Selected " + wxString::FromUTF8(proxyName) + " for " +
              wxString::FromUTF8(selectedProxyGroup_) + "\n");
}

void MainFrame::OnConnectApi(wxCommandEvent&)
{
    corePath_ = corePathText_ ? corePathText_->GetValue().ToStdString() : std::string{};
    configPath_ = configPathText_ ? configPathText_->GetValue().ToStdString() : configPath_;

    std::string runtimeConfigPath;
    std::string settingsError;
    if (!PrepareRuntimeConfig(runtimeConfigPath, settingsError))
    {
        wxMessageBox(wxString::FromUTF8(settingsError), "Invalid Mihomo settings",
                     wxOK | wxICON_ERROR, this);
        AppendLog("[error] " + wxString::FromUTF8(settingsError) + "\n");
        return;
    }

    auto controller = controllerText_ ? controllerText_->GetValue().ToStdString()
                                      : mihomoConfig_.externalController;
    std::string controllerError;
    if (!ValidateExternalController(controller, controllerError))
    {
        if (controllerText_)
        {
            controllerText_->SetFocus();
            controllerText_->SelectAll();
        }
        wxMessageBox(wxString::FromUTF8(controllerError), "Invalid external controller",
                     wxOK | wxICON_ERROR, this);
        AppendLog("[error] " + wxString::FromUTF8(controllerError) + "\n");
        return;
    }
    mihomoConfig_.externalController = controller;
    if (controller.rfind("http://", 0) != 0)
        controller = "http://" + controller;
    apiClient_.SetBaseUrl(controller);
    apiClient_.SetSecret(mihomoConfig_.secret);
    apiClient_.SetTimeoutMs(3000);

    std::string startError;
    if (!mihomoSidecar_.Start(corePath_, dataPath_, runtimeConfigPath,
                              apiClient_, this, startError))
    {
        std::cerr << "[WxClash] Sidecar error: " << startError << std::endl;
        apiConnected_ = false;
        AppendLog("[error] Sidecar error: " + wxString::FromUTF8(startError) + "\n");
        return;
    }

    const auto response = apiClient_.GetVersion();
    if (!response.ok)
    {
        std::cerr << "[WxClash] API error: " << response.error << std::endl;
        apiConnected_ = false;
        mihomoSidecar_.RequestStop();
        AppendLog("[error] API error: " + wxString::FromUTF8(response.error) + "\n");
        return;
    }
    if (overviewStatus_)
        overviewStatus_->SetLabel("API connected");
    try
    {
        const auto versionRoot = Json::parse(response.body);
        const auto version = versionRoot.find("version");
        if (version != versionRoot.end() && version->is_string())
            if (overviewVersion_)
                overviewVersion_->SetLabel(
                    "Mihomo version: " + wxString::FromUTF8(version->get<std::string>()));
    }
    catch (const Json::parse_error&)
    {
        // A successful controller response is still useful if the optional
        // version payload cannot be decoded.
    }
    apiConnected_ = true;
    sidecarOutputTimer_.Start(200);
    monitorTimer_.Start(2000);
}

void MainFrame::OnDisconnectApi(wxCommandEvent&)
{
    apiConnected_ = false;
    sidecarOutputTimer_.Stop();
    monitorTimer_.Stop();
    if (!mihomoSidecar_.IsRunning())
    {
        if (overviewStatus_)
            overviewStatus_->SetLabel("Not connected");
        if (overviewVersion_)
            overviewVersion_->SetLabel("Mihomo version: -");
        if (overviewConnections_)
            overviewConnections_->SetLabel("Connections: -");
        if (overviewTraffic_)
            overviewTraffic_->SetLabel("Traffic: -");
        return;
    }

    if (overviewStatus_)
        overviewStatus_->SetLabel("Disconnecting...");
    mihomoSidecar_.RequestStop();
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    event.Veto();
    if (closing_)
        return;

    closing_ = true;
    apiConnected_ = false;
    sidecarOutputTimer_.Stop();
    monitorTimer_.Stop();

    if (!mihomoSidecar_.IsRunning())
    {
        Destroy();
        return;
    }

    mihomoSidecar_.RequestStop();
}

void MainFrame::OnBrowseCore(wxCommandEvent&)
{
    wxFileDialog dialog(this, "Select mihomo core", wxEmptyString,
                        wxEmptyString, "Executable files|*|All files|*.*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK)
    {
        corePath_ = dialog.GetPath().ToStdString();
        if (corePathText_)
            corePathText_->SetValue(dialog.GetPath());
    }
}

void MainFrame::OnBrowseConfig(wxCommandEvent&)
{
    const wxFileName currentPath(configPathText_ ? configPathText_->GetValue()
                                                  : wxString{});
    wxFileDialog dialog(this, "Select mihomo config",
                        currentPath.GetPath(), currentPath.GetFullName(),
                        "YAML files (*.yaml;*.yml)|*.yaml;*.yml|All files|*.*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;

    std::string error;
    if (!mihomoConfig_.Load(dialog.GetPath().ToStdString(), error))
    {
        AppendLog("[error] " + wxString::FromUTF8(error) + "\n");
        return;
    }
    if (!mihomoConfig_.LoadOverrides(MihomoOverridesPath(dataPath_), error))
    {
        AppendLog("[error] " + wxString::FromUTF8(error) + "\n");
        return;
    }
    configPath_ = dialog.GetPath().ToStdString();
    if (configPathText_)
        configPathText_->SetValue(dialog.GetPath());
    UpdateMihomoControls();
    SaveSettings(corePath_, configPath_, maxLogLength_);
    if (overviewStatus_)
        overviewStatus_->SetLabel("Base config selected: " + dialog.GetFilename());
}

bool MainFrame::SaveMihomoSettings(std::string& error)
{
    mihomoConfig_.mode = mihomoModeChoice_->GetStringSelection().ToStdString();
    mihomoConfig_.logLevel = mihomoLogLevelChoice_->GetStringSelection().ToStdString();
    mihomoConfig_.tunStack = tunStackChoice_->GetStringSelection().ToStdString();
    mihomoConfig_.dnsEnhancedMode = dnsModeChoice_->GetStringSelection().ToStdString();
    mihomoConfig_.externalController = controllerText_->GetValue().ToStdString();
    std::string controllerError;
    if (!ValidateExternalController(mihomoConfig_.externalController, controllerError))
    {
        controllerText_->SetFocus();
        controllerText_->SelectAll();
        wxMessageBox(wxString::FromUTF8(controllerError), "Invalid external controller",
                     wxOK | wxICON_ERROR, this);
        AppendLog("[error] " + wxString::FromUTF8(controllerError) + "\n");
        return false;
    }
    mihomoConfig_.secret = secretText_->GetValue().ToStdString();
    mihomoConfig_.allowLan = allowLanCheck_->GetValue();
    mihomoConfig_.ipv6 = ipv6Check_->GetValue();
    mihomoConfig_.tunEnable = tunEnableCheck_->GetValue();
    mihomoConfig_.dnsEnable = dnsEnableCheck_->GetValue();

    const auto readPort = [](wxTextCtrl* control, int current) {
        long value = 0;
        return control->GetValue().ToLong(&value) && value >= 0 && value <= 65535
                   ? static_cast<int>(value)
                   : current;
    };
    mihomoConfig_.mixedPort = readPort(mixedPortText_, mihomoConfig_.mixedPort);
    mihomoConfig_.httpPort = readPort(httpPortText_, mihomoConfig_.httpPort);
    mihomoConfig_.socksPort = readPort(socksPortText_, mihomoConfig_.socksPort);

    mihomoConfig_.dnsNameservers.clear();
    wxStringTokenizer tokens(nameserverText_->GetValue(), "\n", wxTOKEN_STRTOK);
    while (tokens.HasMoreTokens())
    {
        const auto value = tokens.GetNextToken().Trim(true).Trim(false);
        if (!value.empty())
            mihomoConfig_.dnsNameservers.push_back(value.ToStdString());
    }
    if (mihomoConfig_.dnsNameservers.empty())
        mihomoConfig_.dnsNameservers = {"223.5.5.5", "8.8.8.8"};

    return mihomoConfig_.SaveOverrides(MihomoOverridesPath(dataPath_), error);
}

bool MainFrame::PrepareRuntimeConfig(std::string& runtimePath, std::string& error)
{
    dataPath_ = MihomoSidecar::DefaultDataPath();
    if (!SaveMihomoSettings(error))
        return false;
    runtimePath = MihomoRuntimePath(dataPath_);
    return mihomoConfig_.Save(runtimePath, error);
}

void MainFrame::UpdateMihomoControls()
{
    const auto selectChoice = [](wxChoice* choice, const std::string& value) {
        const int index = choice->FindString(wxString::FromUTF8(value));
        choice->SetSelection(index == wxNOT_FOUND ? 0 : index);
    };
    selectChoice(mihomoModeChoice_, mihomoConfig_.mode);
    selectChoice(mihomoLogLevelChoice_, mihomoConfig_.logLevel);
    selectChoice(tunStackChoice_, mihomoConfig_.tunStack);
    selectChoice(dnsModeChoice_, mihomoConfig_.dnsEnhancedMode);
    mixedPortText_->SetValue(std::to_string(mihomoConfig_.mixedPort));
    httpPortText_->SetValue(std::to_string(mihomoConfig_.httpPort));
    socksPortText_->SetValue(std::to_string(mihomoConfig_.socksPort));
    controllerText_->SetValue(wxString::FromUTF8(mihomoConfig_.externalController));
    secretText_->SetValue(wxString::FromUTF8(mihomoConfig_.secret));
    allowLanCheck_->SetValue(mihomoConfig_.allowLan);
    ipv6Check_->SetValue(mihomoConfig_.ipv6);
    tunEnableCheck_->SetValue(mihomoConfig_.tunEnable);
    dnsEnableCheck_->SetValue(mihomoConfig_.dnsEnable);
    wxString nameservers;
    for (const auto& nameserver : mihomoConfig_.dnsNameservers)
        nameservers += wxString::FromUTF8(nameserver) + "\n";
    nameserverText_->SetValue(nameservers);
}
