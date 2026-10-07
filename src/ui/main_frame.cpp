#include "main_frame.h"

#include <nlohmann/json.hpp>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dir.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/ffile.h>
#include <wx/dataview.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/filename.h>
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

    void LoadSettings(std::string& corePath, std::string& dataPath,
                      std::string& configPath, int& pollingIntervalMs,
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
            else if (line.StartsWith("data_path="))
                dataPath = line.Mid(10).ToStdString();
            else if (line.StartsWith("config_path="))
                configPath = line.Mid(12).ToStdString();
            else if (line.StartsWith("polling_interval_ms="))
            {
                long value = 0;
                if (line.Mid(20).ToLong(&value) &&
                    (value == 1000 || value == 2000 || value == 5000 || value == 10000))
                    pollingIntervalMs = static_cast<int>(value);
            }
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

    void SaveSettings(const std::string& corePath, const std::string& dataPath,
                      const std::string& configPath, int pollingIntervalMs,
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
                                  "\ndata_path=" + wxString::FromUTF8(dataPath) +
                                  "\nconfig_path=" + wxString::FromUTF8(configPath) +
                                  "\npolling_interval_ms=" + wxString::Format("%d", pollingIntervalMs) +
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
    EVT_CLOSE(MainFrame::OnClose)
    EVT_BUTTON(wxID_HIGHEST + 4, MainFrame::OnConnectApi)
    EVT_BUTTON(wxID_HIGHEST + 11, MainFrame::OnDisconnectApi)
    EVT_TIMER(wxID_HIGHEST + 8, MainFrame::OnSidecarOutput)
    EVT_TIMER(wxID_HIGHEST + 10, MainFrame::OnMonitorTimer)
    EVT_LISTBOX(wxID_ANY, MainFrame::OnProxyGroupSelected)
    EVT_DATAVIEW_SELECTION_CHANGED(wxID_ANY, MainFrame::OnProxySelected)
    EVT_BUTTON(wxID_HIGHEST + 5, MainFrame::OnBrowseCore)
    EVT_BUTTON(wxID_HIGHEST + 6, MainFrame::OnBrowseDataPath)
    EVT_BUTTON(wxID_HIGHEST + 7, MainFrame::OnBrowseConfig)
    EVT_BUTTON(wxID_HIGHEST + 9, MainFrame::OnSaveMihomoConfig)
    EVT_BUTTON(wxID_ANY, MainFrame::OnNavigation)
        EVT_CHOICE(wxID_ANY, MainFrame::OnModeChanged)
            wxEND_EVENT_TABLE()

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "WxClash", wxDefaultPosition, wxSize(1100, 700),
      wxDEFAULT_FRAME_STYLE),
      sidecarOutputTimer_(this, wxID_HIGHEST + 8),
      monitorTimer_(this, wxID_HIGHEST + 10)
{
    LoadSettings(corePath_, dataPath_, configPath_, pollingIntervalMs_, maxLogLength_);
    if (dataPath_.empty())
        dataPath_ = MihomoSidecar::DefaultDataPath();
    if (configPath_.empty())
        configPath_ = wxFileName(wxString::FromUTF8(dataPath_), "config.yaml")
                          .GetFullPath().ToStdString();
    std::string configError;
    if (!mihomoConfig_.Load(configPath_, configError))
        std::cerr << "[WxClash] " << configError << std::endl;

    mihomoSidecar_.SetOutputCallback([this](const std::string& message) {
        AppendLog(wxString::FromUTF8(message) + "\n");
    });
    mihomoSidecar_.SetTerminationCallback([this] {
        sidecarOutputTimer_.Stop();
        monitorTimer_.Stop();
        if (closing_)
            Destroy();
        else
        {
            SetStatusText("Not connected", 0);
            SetStatusText("", 1);
            SetStatusText("", 2);
        }
    });
    mihomoSidecar_.SetStartCallback([this] {
        sidecarOutputTimer_.Start(pollingIntervalMs_);
        monitorTimer_.Start(pollingIntervalMs_);
    });
    ApplyPollingSettings();

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
    SetStatusText("Not connected", 0);
    SetStatusText("", 1);
    SetStatusText("Not connected", 2);
    SetSizer(rootSizer);
    Centre();
}

MainFrame::~MainFrame()
{
    if (configPathText_)
        configPath_ = configPathText_->GetValue().ToStdString();
    SaveSettings(corePath_, dataPath_, configPath_, pollingIntervalMs_, maxLogLength_);
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

    auto *shortcuts = new wxBoxSizer(wxHORIZONTAL);
    shortcuts->Add(new wxButton(overview, wxID_HIGHEST + 4, "Connect API"), 0,
                   wxRIGHT, kSpacing);
    shortcuts->Add(new wxButton(overview, wxID_HIGHEST + 11, "Disconnect"), 0);
    overviewSizer->Add(shortcuts, 0);

    auto *proxies = AddPage(book_, "Proxies");
    auto *proxySizer = proxies->GetSizer();
    auto *proxySplit = new wxBoxSizer(wxHORIZONTAL);
    proxyGroups_ = new wxListBox(proxies, wxID_ANY);
    proxySplit->Add(proxyGroups_, 0, wxEXPAND | wxRIGHT, kSpacing);
    proxyTable_ = new wxDataViewListCtrl(proxies, wxID_ANY);
    AddTableColumn(proxyTable_, "Name", 190);
    AddTableColumn(proxyTable_, "Type", 110);
    AddTableColumn(proxyTable_, "Delay", 90);
    AddTableColumn(proxyTable_, "Status", 90);
    proxySplit->Add(proxyTable_, 1, wxEXPAND);
    proxySizer->Add(new wxSearchCtrl(proxies, wxID_ANY), 0,
                    wxEXPAND | wxBOTTOM, kSpacing);
    proxySizer->Add(proxySplit, 1, wxEXPAND);

    auto *connections = AddPage(book_, "Connections");
    auto *connectionSizer = connections->GetSizer();
    connectionSizer->Add(new wxSearchCtrl(connections, wxID_ANY), 0,
                         wxEXPAND | wxBOTTOM, kSpacing);
    connectionTable_ = new wxDataViewListCtrl(connections, wxID_ANY);
    AddTableColumn(connectionTable_, "Target", 220);
    AddTableColumn(connectionTable_, "Process", 160);
    AddTableColumn(connectionTable_, "Network", 80);
    AddTableColumn(connectionTable_, "Rule", 150);
    AddTableColumn(connectionTable_, "Proxy chain", 180);
    connectionSizer->Add(connectionTable_, 1, wxEXPAND);

    auto *rules = AddPage(book_, "Rules");
    auto *ruleSizer = rules->GetSizer();
    ruleSizer->Add(new wxStaticText(rules, wxID_ANY,
                                    "Disabling a rule only affects the current runtime and is lost after restart."),
                   0, wxBOTTOM, kSpacing);
    ruleTable_ = new wxDataViewListCtrl(rules, wxID_ANY);
    AddTableColumn(ruleTable_, "Index", 70);
    AddTableColumn(ruleTable_, "Type", 120);
    AddTableColumn(ruleTable_, "Match", 300);
    AddTableColumn(ruleTable_, "Policy", 150);
    ruleSizer->Add(ruleTable_, 1, wxEXPAND);

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
    connectionForm->Add(new wxStaticText(connectionPage, wxID_ANY, "App data directory"));
    auto *dataPathSizer = new wxBoxSizer(wxHORIZONTAL);
    dataPathText_ = new wxTextCtrl(connectionPage, wxID_ANY,
                                    wxString::FromUTF8(dataPath_),
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_PROCESS_ENTER);
    dataPathSizer->Add(dataPathText_, 1, wxEXPAND | wxRIGHT, kSpacing);
    dataPathSizer->Add(new wxButton(connectionPage, wxID_HIGHEST + 6, "Browse..."),
                        0, wxEXPAND);
    connectionForm->Add(dataPathSizer, 1, wxEXPAND);
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
    connectionPage->SetSizer(connectionForm);
    notebook->AddPage(connectionPage, "Connection");

    auto *mihomoPage = new wxPanel(notebook);
    auto *mihomoPageSizer = new wxBoxSizer(wxVERTICAL);
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
    mihomoPageSizer->Add(mihomoForm, 1, wxEXPAND);

    auto *configActions = new wxBoxSizer(wxHORIZONTAL);
    configActions->Add(new wxButton(mihomoPage, wxID_HIGHEST + 7, "Import"),
                       0);
    configActions->AddStretchSpacer(1);
    configActions->Add(new wxButton(mihomoPage, wxID_HIGHEST + 9, "Save"),
                       0);
    mihomoPageSizer->Add(configActions, 0, wxEXPAND | wxTOP, kSpacing);
    mihomoPage->SetSizer(mihomoPageSizer);
    notebook->AddPage(mihomoPage, "Mihomo");

    auto *displayPage = new wxPanel(notebook);
    auto *displayForm = new wxFlexGridSizer(2, kSpacing, kSpacing);
    pollingIntervalChoice_ = new wxChoice(displayPage, wxID_ANY);
    pollingIntervalChoice_->Append("1 second");
    pollingIntervalChoice_->Append("2 seconds");
    pollingIntervalChoice_->Append("5 seconds");
    pollingIntervalChoice_->Append("10 seconds");
    logLengthChoice_ = new wxChoice(displayPage, wxID_ANY);
    logLengthChoice_->Append("10,000 characters");
    logLengthChoice_->Append("50,000 characters");
    logLengthChoice_->Append("100,000 characters");
    logLengthChoice_->Append("500,000 characters");
    logLengthChoice_->Append("1,000,000 characters");
    displayForm->Add(new wxStaticText(displayPage, wxID_ANY, "Monitor interval"));
    displayForm->Add(pollingIntervalChoice_, 1, wxEXPAND);
    displayForm->Add(new wxStaticText(displayPage, wxID_ANY, "Maximum log length"));
    displayForm->Add(logLengthChoice_, 1, wxEXPAND);
    displayForm->AddGrowableCol(1, 1);
    displayPage->SetSizer(displayForm);
    notebook->AddPage(displayPage, "Display");

    UpdateMihomoControls();
    const int pollingValues[] = {1000, 2000, 5000, 10000};
    const std::size_t logLengthValues[] = {10000, 50000, 100000, 500000, 1000000};
    for (unsigned int index = 0; index < 4; ++index)
        if (pollingValues[index] == pollingIntervalMs_)
            pollingIntervalChoice_->SetSelection(index);
    for (unsigned int index = 0; index < 5; ++index)
        if (logLengthValues[index] == maxLogLength_)
            logLengthChoice_->SetSelection(index);
    if (pollingIntervalChoice_->GetSelection() == wxNOT_FOUND)
        pollingIntervalChoice_->SetSelection(1);
    if (logLengthChoice_->GetSelection() == wxNOT_FOUND)
        logLengthChoice_->SetSelection(2);
    ApplyPollingSettings();
    settingsSizer->Add(notebook, 1, wxEXPAND);

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
        else if (page == PageRules)
            RefreshRules();
    }
}

void MainFrame::OnModeChanged(wxCommandEvent &event)
{
    if (event.GetEventObject() == pollingIntervalChoice_ ||
        event.GetEventObject() == logLengthChoice_)
    {
        const int pollingValues[] = {1000, 2000, 5000, 10000};
        const std::size_t logLengthValues[] = {10000, 50000, 100000, 500000, 1000000};
        if (pollingIntervalChoice_->GetSelection() >= 0)
            pollingIntervalMs_ = pollingValues[pollingIntervalChoice_->GetSelection()];
        if (logLengthChoice_->GetSelection() >= 0)
            maxLogLength_ = logLengthValues[logLengthChoice_->GetSelection()];
        ApplyPollingSettings();
        AppendLog(wxString::Format("[info] Display settings updated; log limit: %zu characters\n",
                                   maxLogLength_));
        return;
    }
}

void MainFrame::OnSidecarOutput(wxTimerEvent&)
{
    mihomoSidecar_.PollOutput();
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

void MainFrame::ApplyPollingSettings()
{
    if (sidecarOutputTimer_.IsRunning())
        sidecarOutputTimer_.Start(pollingIntervalMs_);
    if (monitorTimer_.IsRunning())
        monitorTimer_.Start(pollingIntervalMs_);
}

void MainFrame::OnMonitorTimer(wxTimerEvent&)
{
    if (!apiConnected_)
        return;
    RefreshCoreData();
}

void MainFrame::RefreshCoreData()
{
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
        SetStatusText(wxString::Format("Connections: %zu", connectionCount), 1);

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
            SetStatusText("Download: " +
                              wxString::FromUTF8(FormatBytes(JsonUint64(&traffic, "down"))) +
                              "/s  Upload: " +
                              wxString::FromUTF8(FormatBytes(JsonUint64(&traffic, "up"))) +
                              "/s",
                          2);
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
        return;
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
    const auto proxies = root.find("proxies");
    if (proxyGroups_)
        proxyGroups_->Clear();
    if (proxies != root.end() && proxies->is_object())
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
            proxyCurrentSelection_[name] = JsonString(&proxy, "now");
            if (proxyGroups_)
                proxyGroups_->Append(wxString::FromUTF8(name));
        }
    }
    if (proxyGroups_ && proxyGroups_->GetCount() > 0)
    {
        int selection = proxyGroups_->FindString(wxString::FromUTF8(selectedProxyGroup_));
        if (selection == wxNOT_FOUND)
            selection = 0;
        proxyGroups_->SetSelection(selection);
        selectedProxyGroup_ = proxyGroups_->GetString(selection).ToStdString();
    }
    else
        selectedProxyGroup_.clear();
    PopulateProxyTable();
}

void MainFrame::OnProxyGroupSelected(wxCommandEvent& event)
{
    if (event.GetEventObject() != proxyGroups_ || !proxyGroups_)
        return;
    const int selection = proxyGroups_->GetSelection();
    if (selection == wxNOT_FOUND)
        return;
    selectedProxyGroup_ = proxyGroups_->GetString(selection).ToStdString();
    PopulateProxyTable();
}

void MainFrame::PopulateProxyTable()
{
    if (!proxyTable_)
        return;
    proxyTable_->DeleteAllItems();
    const auto proxies = proxyData_.find("proxies");
    if (proxies == proxyData_.end() || !proxies->is_object())
        return;
    const auto members = proxyGroupMembers_.find(selectedProxyGroup_);
    if (members == proxyGroupMembers_.end())
        return;
    const auto current = proxyCurrentSelection_.find(selectedProxyGroup_);
    for (const auto& name : members->second)
    {
        const auto proxy = proxies->find(name);
        if (proxy == proxies->end())
            continue;
        std::string delay = JsonString(&(*proxy), "now");
        const auto history = proxy->find("history");
        if (delay.empty() && history != proxy->end() && history->is_array() && !history->empty())
        {
            const auto& latest = history->back();
            if (latest.contains("delay"))
            {
                try { delay = std::to_string(latest["delay"].get<std::uint64_t>()); }
                catch (const Json::type_error&) { delay = "-"; }
            }
        }
        if (delay.empty())
            delay = "-";
        const bool alive = !proxy->contains("alive") || (*proxy)["alive"].get<bool>();
        const bool selected = current != proxyCurrentSelection_.end() &&
                              current->second == name;
        wxVector<wxVariant> values;
        values.push_back(selected ? wxString::FromUTF8("✓ ") + wxString::FromUTF8(name)
                                  : wxString::FromUTF8(name));
        values.push_back(wxString::FromUTF8(JsonString(&(*proxy), "type")));
        values.push_back(wxString::FromUTF8(delay));
        values.push_back(selected ? wxString("Selected")
                                  : wxString(alive ? "Available" : "Unavailable"));
        proxyTable_->AppendItem(values);
    }
}

void MainFrame::OnProxySelected(wxDataViewEvent& event)
{
    if (event.GetEventObject() != proxyTable_ || !proxyTable_ || selectedProxyGroup_.empty())
        return;
    const int row = proxyTable_->ItemToRow(event.GetItem());
    if (row == wxNOT_FOUND)
        return;
    SelectProxy();
}

void MainFrame::SelectProxy()
{
    if (!proxyTable_ || selectedProxyGroup_.empty())
        return;
    const int row = proxyTable_->GetSelectedRow();
    if (row == wxNOT_FOUND)
        return;
    auto proxyName = proxyTable_->GetTextValue(row, 0).ToStdString();
    if (proxyName.rfind("✓ ", 0) == 0)
        proxyName.erase(0, 2);
    const auto response = apiClient_.SelectProxy(selectedProxyGroup_, proxyName);
    if (!response.ok)
    {
        AppendLog("[error] Proxy selection failed: " + wxString::FromUTF8(response.error) + "\n");
        return;
    }
    proxyCurrentSelection_[selectedProxyGroup_] = proxyName;
    PopulateProxyTable();
    AppendLog("[info] Selected " + wxString::FromUTF8(proxyName) + " for " +
              wxString::FromUTF8(selectedProxyGroup_) + "\n");
}

void MainFrame::RefreshRules()
{
    if (!apiConnected_)
        return;
    const auto response = apiClient_.GetRules();
    if (!response.ok)
    {
        AppendLog("[error] Monitor error (/rules): " +
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
        AppendLog("[error] Invalid /rules JSON: " +
                  wxString::FromUTF8(exception.what()) + "\n");
        return;
    }
    if (!root.is_object())
    {
        AppendLog("[error] Invalid /rules JSON: expected an object\n");
        return;
    }
    {
        const auto rules = root.find("rules");
        if (ruleTable_)
        {
            ruleTable_->DeleteAllItems();
            if (rules != root.end() && rules->is_array())
            {
                for (const auto& rule : *rules)
                    ruleTable_->AppendItem({std::to_string(JsonUint64(&rule, "index")),
                                            JsonString(&rule, "type"),
                                            JsonString(&rule, "payload"),
                                            JsonString(&rule, "proxy")});
            }
        }
    }
}

void MainFrame::OnConnectApi(wxCommandEvent&)
{
    corePath_ = corePathText_ ? corePathText_->GetValue().ToStdString() : std::string{};
    dataPath_ = dataPathText_ ? dataPathText_->GetValue().ToStdString() : std::string{};
    configPath_ = configPathText_ ? configPathText_->GetValue().ToStdString() : configPath_;

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
    apiClient_.SetTimeoutMs(1000);

    std::string startError;
    if (!mihomoSidecar_.Start(corePath_, dataPath_, configPath_,
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
    SetStatusText("API connected", 0);
    try
    {
        const auto versionRoot = Json::parse(response.body);
        const auto version = versionRoot.find("version");
        if (version != versionRoot.end() && version->is_string())
            SetStatusText("Mihomo " + wxString::FromUTF8(version->get<std::string>()), 0);
    }
    catch (const Json::parse_error&)
    {
        // A successful controller response is still useful if the optional
        // version payload cannot be decoded.
    }
    apiConnected_ = true;
    RefreshCoreData();
}

void MainFrame::OnDisconnectApi(wxCommandEvent&)
{
    apiConnected_ = false;
    if (!mihomoSidecar_.IsRunning())
    {
        SetStatusText("Not connected", 0);
        SetStatusText("", 1);
        SetStatusText("", 2);
        return;
    }

    SetStatusText("Disconnecting...", 0);
    mihomoSidecar_.RequestStop();
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    event.Veto();
    if (closing_)
        return;

    closing_ = true;
    apiConnected_ = false;

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

void MainFrame::OnBrowseDataPath(wxCommandEvent&)
{
    const wxString currentPath = dataPathText_ ? dataPathText_->GetValue() : wxString{};
    wxDirDialog dialog(this, "Select WxClash data directory",
                       currentPath,
                       wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK)
    {
        dataPath_ = dialog.GetPath().ToStdString();
        if (dataPathText_)
            dataPathText_->SetValue(dialog.GetPath());
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
    configPath_ = dialog.GetPath().ToStdString();
    if (configPathText_)
        configPathText_->SetValue(dialog.GetPath());
    UpdateMihomoControls();
    SaveSettings(corePath_, dataPath_, configPath_, pollingIntervalMs_, maxLogLength_);
    SetStatusText("Config imported: " + dialog.GetFilename(), 2);
}

void MainFrame::OnSaveMihomoConfig(wxCommandEvent&)
{
    dataPath_ = dataPathText_ ? dataPathText_->GetValue().ToStdString() : dataPath_;
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
        return;
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

    const wxFileName currentPath(configPathText_ ? configPathText_->GetValue()
                                                  : wxString::FromUTF8(configPath_));
    wxFileDialog dialog(this, "Save mihomo config", currentPath.GetPath(),
                        currentPath.GetFullName(),
                        "YAML files (*.yaml;*.yml)|*.yaml;*.yml|All files|*.*",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
        return;

    const auto path = dialog.GetPath().ToStdString();
    std::string error;
    if (!mihomoConfig_.Save(path, error))
    {
        std::cerr << "[WxClash] " << error << std::endl;
        AppendLog("[error] " + wxString::FromUTF8(error) + "\n");
        return;
    }
    configPath_ = path;
    if (configPathText_)
        configPathText_->SetValue(dialog.GetPath());
    SaveSettings(corePath_, dataPath_, configPath_, pollingIntervalMs_, maxLogLength_);
    std::cerr << "[WxClash] Saved mihomo config: " << path << std::endl;
    SetStatusText("Mihomo config saved", 2);
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
