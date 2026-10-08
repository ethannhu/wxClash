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
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tokenzr.h>

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

}

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_CLOSE(MainFrame::OnClose)
    EVT_TIMER(static_cast<int>(ControlId::SidecarOutputTimer), MainFrame::OnSidecarOutput)
    EVT_TIMER(static_cast<int>(ControlId::MonitorTimer), MainFrame::OnMonitorTimer)
    EVT_BUTTON(static_cast<int>(ControlId::ConnectApi), MainFrame::OnConnectApi)
    EVT_BUTTON(static_cast<int>(ControlId::DisconnectApi), MainFrame::OnDisconnectApi)
    EVT_RADIOBOX(static_cast<int>(ControlId::ProxyGroup), MainFrame::OnProxyGroupSelected)
    EVT_RADIOBUTTON(static_cast<int>(ControlId::ProxyChoice), MainFrame::OnProxySelected)
    EVT_BUTTON(static_cast<int>(ControlId::BrowseCore), MainFrame::OnBrowseCore)
    EVT_BUTTON(static_cast<int>(ControlId::BrowseConfig), MainFrame::OnBrowseConfig)
    EVT_BUTTON(wxID_ANY, MainFrame::OnNavigation)
        EVT_CHOICE(wxID_ANY, MainFrame::OnModeChanged)
            wxEND_EVENT_TABLE()

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "WxClash", wxDefaultPosition, wxSize(1100, 700),
      wxDEFAULT_FRAME_STYLE),
      sidecarOutputTimer_(this, static_cast<int>(ControlId::SidecarOutputTimer)),
      monitorTimer_(this, static_cast<int>(ControlId::MonitorTimer))
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
            overviewPage_->status->SetLabel("Not connected");
            overviewPage_->version->SetLabel("Mihomo version: -");
            overviewPage_->connections->SetLabel("Connections: -");
            overviewPage_->traffic->SetLabel("Traffic: -");
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
    if (mihomoPage_)
    {
        std::string ignoredError;
        SaveMihomoSettings(ignoredError);
    }
    configPath_ = settingsPage_->configPath->GetValue().ToStdString();
    SaveSettings(corePath_, configPath_, maxLogLength_);
}

void MainFrame::BuildNavigation(wxSizer *parentSizer)
{
    auto *panel = new wxPanel(this, wxID_ANY, wxDefaultPosition,
                              wxSize(kNavigationWidth, -1));
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    AddNavigationButton(panel, sizer, "Overview", PageId::Overview);
    AddNavigationButton(panel, sizer, "Proxies", PageId::Proxies);
    AddNavigationButton(panel, sizer, "Connections", PageId::Connections);
    AddNavigationButton(panel, sizer, "Logs", PageId::Logs);
    AddNavigationButton(panel, sizer, "Settings", PageId::Settings);
    AddNavigationButton(panel, sizer, "Mihomo", PageId::Mihomo);
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
    overviewPage_ = new OverviewPage(book_);
    proxyPage_ = new ProxyPage(book_);
    connectionsPage_ = new ConnectionsPage(book_);
    logsPage_ = new LogsPage(book_);
    settingsPage_ = new SettingsPage(book_);
    mihomoPage_ = new MihomoPage(book_);

    book_->AddPage(overviewPage_, "Overview");
    book_->AddPage(proxyPage_, "Proxies");
    book_->AddPage(connectionsPage_, "Connections");
    book_->AddPage(logsPage_, "Logs");
    book_->AddPage(settingsPage_, "Settings");
    book_->AddPage(mihomoPage_, "Mihomo");

    const std::size_t logLengthValues[] = {10000, 50000, 100000, 500000, 1000000};
    for (unsigned int index = 0; index < 5; ++index)
    {
        if (logLengthValues[index] == maxLogLength_)
            settingsPage_->logLength->SetSelection(index);
    }
    if (settingsPage_->logLength->GetSelection() == wxNOT_FOUND)
        settingsPage_->logLength->SetSelection(2);

    if (!corePath_.empty())
        settingsPage_->corePath->SetValue(wxString::FromUTF8(corePath_));
    if (!configPath_.empty())
        settingsPage_->configPath->SetValue(wxString::FromUTF8(configPath_));

    UpdateMihomoControls();
}

void MainFrame::OnNavigation(wxCommandEvent &event)
{
    const auto page = event.GetId();
    if (page >= 0 && page < static_cast<int>(PageId::Count))
    {
        book_->SetSelection(page);
        if (page == static_cast<int>(PageId::Proxies))
            RefreshProxies();
    }
}

void MainFrame::OnModeChanged(wxCommandEvent &event)
{
    if (event.GetEventObject() == settingsPage_->logLength)
    {
        const std::size_t logLengthValues[] = {10000, 50000, 100000, 500000, 1000000};
        if (settingsPage_->logLength->GetSelection() >= 0)
            maxLogLength_ = logLengthValues[settingsPage_->logLength->GetSelection()];
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
    if (page == static_cast<int>(PageId::Overview) ||
        page == static_cast<int>(PageId::Connections))
        RefreshCoreData();
}

void MainFrame::AppendLog(const wxString& message)
{
    logsPage_->text->AppendText(message);
    const auto length = static_cast<std::size_t>(logsPage_->text->GetLastPosition());
    if (length > maxLogLength_)
        logsPage_->text->Remove(0, static_cast<long>(length - maxLogLength_));
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
        overviewPage_->connections->SetLabel(
            wxString::Format("Connections: %zu", connectionCount));

        connectionsPage_->table->DeleteAllItems();
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
                    connectionsPage_->table->AppendItem(
                        {target,
                         JsonString(metadataObject, "process"),
                         JsonString(metadataObject, "network"),
                         JsonString(&connection, "rule"),
                         chains});
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
            overviewPage_->traffic->SetLabel(
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
    auto* groupSizer = proxyPage_->groupsPane->GetSizer();
    if (groupSizer)
    {
        groupSizer->Detach(proxyPage_->groups);
        proxyPage_->groups->Destroy();
        wxArrayString labels;
        for (const auto& name : proxyGroupNames_)
            labels.Add(wxString::FromUTF8(name));
        if (labels.empty())
            labels.Add("No groups");
        proxyPage_->groups = new wxRadioBox(
            proxyPage_->groupsPane, static_cast<int>(ControlId::ProxyGroup),
            "Groups",
            wxDefaultPosition, wxDefaultSize, labels, 1, wxRA_SPECIFY_COLS);
        groupSizer->Add(proxyPage_->groups, 1, wxEXPAND);
        proxyPage_->groupsPane->Layout();
    }
    if (!proxyGroupNames_.empty())
    {
        int selection = proxyPage_->groups->FindString(
            wxString::FromUTF8(selectedProxyGroup_));
        if (selection == wxNOT_FOUND)
            selection = 0;
        proxyPage_->groups->SetSelection(selection);
        selectedProxyGroup_ = proxyGroupNames_[static_cast<std::size_t>(selection)];
    }
    else
        selectedProxyGroup_.clear();
    updatingProxyGroups_ = false;
    PopulateProxyChoices();
}

void MainFrame::OnProxyGroupSelected(wxCommandEvent& event)
{
    if (event.GetEventObject() != proxyPage_->groups || !proxyPage_->groups)
        return;
    if (updatingProxyGroups_)
        return;
    const int selection = proxyPage_->groups->GetSelection();
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
    if (!proxyPage_->choicesScroll)
        return;
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
    auto* choiceSizer = proxyPage_->choicesScroll->GetSizer();
    proxyPage_->choiceButtons.clear();
    choiceSizer->Clear(true);
    choiceSizer->Add(new wxStaticText(proxyPage_->choicesScroll, wxID_ANY, "Proxies"),
                     0, wxBOTTOM, kSpacing);
    for (const auto& name : proxyChoiceNames_)
    {
        const auto proxy = proxies->find(name);
        std::string label = name;
        if (proxy != proxies->end())
            label += "  [" + JsonString(&(*proxy), "type", "Proxy") + "]";
        if (current != proxyCurrentSelection_.end() && current->second == name)
            label += "  (selected)";
        auto* button = new wxRadioButton(
            proxyPage_->choicesScroll, static_cast<int>(ControlId::ProxyChoice),
                                         wxString::FromUTF8(label),
                                         wxDefaultPosition, wxDefaultSize,
                                         proxyPage_->choiceButtons.empty()
                                             ? wxRB_GROUP
                                             : 0);
        proxyPage_->choiceButtons.push_back(button);
        choiceSizer->Add(button, 0, wxEXPAND | wxBOTTOM, 4);
    }
    const auto currentName = proxyCurrentSelection_.find(selectedProxyGroup_);
    if (currentName != proxyCurrentSelection_.end())
    {
        const auto selectedIndex = std::find(proxyChoiceNames_.begin(),
                                             proxyChoiceNames_.end(),
                                             currentName->second);
        if (selectedIndex != proxyChoiceNames_.end())
            proxyPage_->choiceButtons[static_cast<std::size_t>(std::distance(
                proxyChoiceNames_.begin(), selectedIndex))]->SetValue(true);
    }
    if (proxyPage_->choiceButtons.empty())
        choiceSizer->Add(new wxStaticText(proxyPage_->choicesScroll,
                                          wxID_ANY, "No proxies"),
                         0, wxEXPAND);
    proxyPage_->choicesScroll->FitInside();
    proxyPage_->choicesScroll->Layout();
    updatingProxyTable_ = false;
}

void MainFrame::OnProxySelected(wxCommandEvent& event)
{
    if (updatingProxyTable_ || selectedProxyGroup_.empty())
        return;
    auto* button = dynamic_cast<wxRadioButton*>(event.GetEventObject());
    const auto it = std::find(proxyPage_->choiceButtons.begin(),
                              proxyPage_->choiceButtons.end(), button);
    if (it == proxyPage_->choiceButtons.end())
        return;
    SelectProxy(proxyChoiceNames_[static_cast<std::size_t>(
        std::distance(proxyPage_->choiceButtons.begin(), it))]);
}

void MainFrame::SelectProxy(const std::string& proxyName)
{
    if (!proxyPage_->choicesScroll || selectedProxyGroup_.empty())
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
    corePath_ = settingsPage_->corePath->GetValue().ToStdString();
    configPath_ = settingsPage_->configPath->GetValue().ToStdString();

    std::string runtimeConfigPath;
    std::string settingsError;
    if (!PrepareRuntimeConfig(runtimeConfigPath, settingsError))
    {
        wxMessageBox(wxString::FromUTF8(settingsError), "Invalid Mihomo settings",
                     wxOK | wxICON_ERROR, this);
        AppendLog("[error] " + wxString::FromUTF8(settingsError) + "\n");
        return;
    }

    auto controller = mihomoPage_->controller->GetValue().ToStdString();
    std::string controllerError;
    if (!ValidateExternalController(controller, controllerError))
    {
        mihomoPage_->controller->SetFocus();
        mihomoPage_->controller->SelectAll();
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
    overviewPage_->status->SetLabel("API connected");
    try
    {
        const auto versionRoot = Json::parse(response.body);
        const auto version = versionRoot.find("version");
        if (version != versionRoot.end() && version->is_string())
            overviewPage_->version->SetLabel(
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
        overviewPage_->status->SetLabel("Not connected");
        overviewPage_->version->SetLabel("Mihomo version: -");
        overviewPage_->connections->SetLabel("Connections: -");
        overviewPage_->traffic->SetLabel("Traffic: -");
        return;
    }

    overviewPage_->status->SetLabel("Disconnecting...");
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
        settingsPage_->corePath->SetValue(dialog.GetPath());
    }
}

void MainFrame::OnBrowseConfig(wxCommandEvent&)
{
    const wxFileName currentPath(settingsPage_->configPath->GetValue());
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
    settingsPage_->configPath->SetValue(dialog.GetPath());
    UpdateMihomoControls();
    SaveSettings(corePath_, configPath_, maxLogLength_);
    overviewPage_->status->SetLabel("Base config selected: " + dialog.GetFilename());
}

bool MainFrame::SaveMihomoSettings(std::string& error)
{
    mihomoConfig_.mode = mihomoPage_->mode->GetStringSelection().ToStdString();
    mihomoConfig_.logLevel = mihomoPage_->logLevel->GetStringSelection().ToStdString();
    mihomoConfig_.tunStack = mihomoPage_->tunStack->GetStringSelection().ToStdString();
    mihomoConfig_.dnsEnhancedMode = mihomoPage_->dnsMode->GetStringSelection().ToStdString();
    mihomoConfig_.externalController = mihomoPage_->controller->GetValue().ToStdString();
    std::string controllerError;
    if (!ValidateExternalController(mihomoConfig_.externalController, controllerError))
    {
        mihomoPage_->controller->SetFocus();
        mihomoPage_->controller->SelectAll();
        wxMessageBox(wxString::FromUTF8(controllerError), "Invalid external controller",
                     wxOK | wxICON_ERROR, this);
        AppendLog("[error] " + wxString::FromUTF8(controllerError) + "\n");
        return false;
    }
    mihomoConfig_.secret = mihomoPage_->secret->GetValue().ToStdString();
    mihomoConfig_.allowLan = mihomoPage_->allowLan->GetValue();
    mihomoConfig_.ipv6 = mihomoPage_->ipv6->GetValue();
    mihomoConfig_.tunEnable = mihomoPage_->tunEnable->GetValue();
    mihomoConfig_.dnsEnable = mihomoPage_->dnsEnable->GetValue();

    const auto readPort = [](wxTextCtrl* control, int current) {
        long value = 0;
        return control->GetValue().ToLong(&value) && value >= 0 && value <= 65535
                   ? static_cast<int>(value)
                   : current;
    };
    mihomoConfig_.mixedPort = readPort(mihomoPage_->mixedPort, mihomoConfig_.mixedPort);
    mihomoConfig_.httpPort = readPort(mihomoPage_->httpPort, mihomoConfig_.httpPort);
    mihomoConfig_.socksPort = readPort(mihomoPage_->socksPort, mihomoConfig_.socksPort);

    mihomoConfig_.dnsNameservers.clear();
    wxStringTokenizer tokens(mihomoPage_->nameservers->GetValue(), "\n", wxTOKEN_STRTOK);
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
    selectChoice(mihomoPage_->mode, mihomoConfig_.mode);
    selectChoice(mihomoPage_->logLevel, mihomoConfig_.logLevel);
    selectChoice(mihomoPage_->tunStack, mihomoConfig_.tunStack);
    selectChoice(mihomoPage_->dnsMode, mihomoConfig_.dnsEnhancedMode);
    mihomoPage_->mixedPort->SetValue(std::to_string(mihomoConfig_.mixedPort));
    mihomoPage_->httpPort->SetValue(std::to_string(mihomoConfig_.httpPort));
    mihomoPage_->socksPort->SetValue(std::to_string(mihomoConfig_.socksPort));
    mihomoPage_->controller->SetValue(wxString::FromUTF8(mihomoConfig_.externalController));
    mihomoPage_->secret->SetValue(wxString::FromUTF8(mihomoConfig_.secret));
    mihomoPage_->allowLan->SetValue(mihomoConfig_.allowLan);
    mihomoPage_->ipv6->SetValue(mihomoConfig_.ipv6);
    mihomoPage_->tunEnable->SetValue(mihomoConfig_.tunEnable);
    mihomoPage_->dnsEnable->SetValue(mihomoConfig_.dnsEnable);
    wxString nameservers;
    for (const auto& nameserver : mihomoConfig_.dnsNameservers)
        nameservers += wxString::FromUTF8(nameserver) + "\n";
    mihomoPage_->nameservers->SetValue(nameservers);
}
