#include "main_frame.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dir.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/ffile.h>
#include <wx/dataview.h>
#include <wx/listbox.h>
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

#include <iostream>

namespace
{
    constexpr int kSpacing = 8;
    constexpr int kPadding = 12;
    constexpr int kNavigationWidth = 150;

    wxString SettingsPath()
    {
        const auto directory = wxString::FromUTF8(MihomoSidecar::DefaultDataPath());
        wxFileName::Mkdir(directory, 0700, wxPATH_MKDIR_FULL);
        return wxFileName(directory, "settings.conf").GetFullPath();
    }

    void LoadSettings(std::string& corePath, std::string& dataPath,
                      std::string& configPath)
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
        }
    }

    void SaveSettings(const std::string& corePath, const std::string& dataPath,
                      const std::string& configPath)
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
                                  "\n";
        if (!file.Write(contents) || !file.Close())
            std::cerr << "[WxClash] Unable to write settings file" << std::endl;
    }

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
    EVT_TIMER(wxID_HIGHEST + 8, MainFrame::OnSidecarOutput)
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
      sidecarOutputTimer_(this, wxID_HIGHEST + 8)
{
    LoadSettings(corePath_, dataPath_, configPath_);
    if (dataPath_.empty())
        dataPath_ = MihomoSidecar::DefaultDataPath();
    if (configPath_.empty())
        configPath_ = wxFileName(wxString::FromUTF8(dataPath_), "config.yaml")
                          .GetFullPath().ToStdString();
    std::string configError;
    if (!mihomoConfig_.Load(configPath_, configError))
        std::cerr << "[WxClash] " << configError << std::endl;

    apiClient_.SetDebugCallback([this](const std::string& message) {
        if (logText_)
            logText_->AppendText(wxString::FromUTF8(message) + "\n\n");
    });
    mihomoSidecar_.SetOutputCallback([this](const std::string& message) {
        if (logText_)
            logText_->AppendText(wxString::FromUTF8(message) + "\n");
    });
    sidecarOutputTimer_.Start(50);

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

MainFrame::~MainFrame()
{
    sidecarOutputTimer_.Stop();
    if (configPathText_)
        configPath_ = configPathText_->GetValue().ToStdString();
    SaveSettings(corePath_, dataPath_, configPath_);
    mihomoSidecar_.Stop();
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

    auto *mihomoPage = new wxPanel(notebook);
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
    mihomoForm->Add(new wxStaticText(mihomoPage, wxID_ANY, wxEmptyString));
    mihomoForm->Add(new wxButton(mihomoPage, wxID_HIGHEST + 9, "Save mihomo config"),
                     0, wxALIGN_RIGHT);
    mihomoForm->AddGrowableCol(1, 1);
    mihomoForm->AddGrowableRow(10, 1);
    mihomoPage->SetSizer(mihomoForm);
    notebook->AddPage(mihomoPage, "Mihomo");

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
        book_->SetSelection(page);
}

void MainFrame::OnModeChanged(wxCommandEvent &event)
{
    if (event.GetEventObject() != modeChoice_)
        return;

    SetStatusText("Mode: " + modeChoice_->GetStringSelection(), 2);
}

void MainFrame::OnSidecarOutput(wxTimerEvent&)
{
    mihomoSidecar_.PollOutput();
}

void MainFrame::OnConnectApi(wxCommandEvent&)
{
    corePath_ = corePathText_ ? corePathText_->GetValue().ToStdString() : std::string{};
    dataPath_ = dataPathText_ ? dataPathText_->GetValue().ToStdString() : std::string{};
    configPath_ = configPathText_ ? configPathText_->GetValue().ToStdString() : configPath_;
    if (configPath_.empty())
        configPath_ = wxFileName(wxString::FromUTF8(dataPath_), "config.yaml")
                          .GetFullPath().ToStdString();
    std::string startError;
    if (!mihomoSidecar_.Start(corePath_, dataPath_, configPath_, mihomoConfig_,
                              apiClient_, startError))
    {
        std::cerr << "[WxClash] Sidecar error: " << startError << std::endl;
        SetStatusText("Sidecar error: " + wxString::FromUTF8(startError), 2);
        return;
    }

    const auto response = apiClient_.GetVersion();
    if (!response.ok)
    {
        std::cerr << "[WxClash] API error: " << response.error << std::endl;
        SetStatusText("API error: " + wxString::FromUTF8(response.error), 2);
        return;
    }
    SetStatusText("API connected", 2);
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

    if (configPathText_)
    {
        configPathText_->SetValue(dialog.GetPath());
        configPath_ = dialog.GetPath().ToStdString();
    }

    dataPath_ = dataPathText_ ? dataPathText_->GetValue().ToStdString() : dataPath_;
    const wxString dataDirectory = wxString::FromUTF8(dataPath_);
    if (dataDirectory.empty() ||
        (!wxFileName::Mkdir(dataDirectory, 0700, wxPATH_MKDIR_FULL) &&
         !wxDir::Exists(dataDirectory)))
    {
        const std::string error = "Unable to create WxClash data directory";
        std::cerr << "[WxClash] " << error << std::endl;
        SetStatusText(wxString::FromUTF8(error), 2);
        return;
    }

    const wxFileName source(dialog.GetPath());
    const wxFileName destination(dataDirectory, source.GetFullName());
    if (!wxCopyFile(source.GetFullPath(), destination.GetFullPath(), true))
    {
        const std::string error = "Unable to copy mihomo config to data directory";
        std::cerr << "[WxClash] " << error << ": "
                  << destination.GetFullPath().ToStdString() << std::endl;
        SetStatusText(wxString::FromUTF8(error), 2);
        return;
    }

    std::cerr << "[WxClash] Imported mihomo config: "
              << destination.GetFullPath().ToStdString() << std::endl;
    SetStatusText("Config imported: " + destination.GetFullName(), 2);
}

void MainFrame::OnSaveMihomoConfig(wxCommandEvent&)
{
    dataPath_ = dataPathText_ ? dataPathText_->GetValue().ToStdString() : dataPath_;
    mihomoConfig_.mode = mihomoModeChoice_->GetStringSelection().ToStdString();
    mihomoConfig_.logLevel = mihomoLogLevelChoice_->GetStringSelection().ToStdString();
    mihomoConfig_.tunStack = tunStackChoice_->GetStringSelection().ToStdString();
    mihomoConfig_.dnsEnhancedMode = dnsModeChoice_->GetStringSelection().ToStdString();
    mihomoConfig_.externalController = controllerText_->GetValue().ToStdString();
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

    const auto path = wxFileName(wxString::FromUTF8(dataPath_), "config.yaml")
                          .GetFullPath().ToStdString();
    std::string error;
    if (!mihomoConfig_.Save(path, error))
    {
        std::cerr << "[WxClash] " << error << std::endl;
        SetStatusText(wxString::FromUTF8(error), 2);
        return;
    }
    if (configPathText_)
        configPath_ = configPathText_->GetValue().ToStdString();
    SaveSettings(corePath_, dataPath_, configPath_);
    std::cerr << "[WxClash] Saved mihomo config: " << path << std::endl;
    SetStatusText("Mihomo config saved", 2);
}
