#include "app_config.h"

#include <wx/filename.h>
#include <wx/textfile.h>

#include <fstream>
#include <sstream>

namespace
{
    std::string Trim(std::string value)
    {
        const auto first = value.find_first_not_of(" \t\r");
        if (first == std::string::npos)
            return {};
        const auto last = value.find_last_not_of(" \t\r");
        return value.substr(first, last - first + 1);
    }

    std::string Unquote(std::string value)
    {
        value = Trim(std::move(value));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            return value.substr(1, value.size() - 2);
        return value;
    }

    bool ParseBool(const std::string& value, bool fallback)
    {
        const auto normalized = Trim(value);
        if (normalized == "true")
            return true;
        if (normalized == "false")
            return false;
        return fallback;
    }

    int ParseInt(const std::string& value, int fallback)
    {
        try
        {
            const auto parsed = std::stoi(Trim(value));
            return parsed >= 0 && parsed <= 65535 ? parsed : fallback;
        }
        catch (...)
        {
            return fallback;
        }
    }

    std::string YamlString(const std::string& value)
    {
        std::string escaped = value;
        std::string result = "\"";
        for (const auto character : escaped)
        {
            if (character == '\\' || character == '"')
                result += '\\';
            result += character;
        }
        result += '"';
        return result;
    }
}

bool MihomoConfig::Load(const std::string& path, std::string& error)
{
    wxTextFile file(wxString::FromUTF8(path));
    if (!file.Exists())
        return true;
    if (!file.Open())
    {
        error = "Unable to read mihomo config file";
        return false;
    }

    dnsNameservers.clear();
    std::string section;
    for (size_t index = 0; index < file.GetLineCount(); ++index)
    {
        auto line = std::string(file.GetLine(index).utf8_str());
        line = Trim(line);
        if (line.empty() || line.front() == '#')
            continue;

        if (line == "tun:")
        {
            section = "tun";
            continue;
        }
        if (line == "dns:")
        {
            section = "dns";
            continue;
        }
        if (line.rfind("- ", 0) == 0 && section == "dns_nameservers")
        {
            dnsNameservers.push_back(Unquote(line.substr(2)));
            continue;
        }

        const auto separator = line.find(':');
        if (separator == std::string::npos)
            continue;

        const auto key = Trim(line.substr(0, separator));
        const auto value = Unquote(line.substr(separator + 1));
        if (section == "tun")
        {
            if (key == "enable")
                tunEnable = ParseBool(value, tunEnable);
            else if (key == "stack")
                tunStack = value;
        }
        else if (section == "dns")
        {
            if (key == "enable")
                dnsEnable = ParseBool(value, dnsEnable);
            else if (key == "enhanced-mode")
                dnsEnhancedMode = value;
            else if (key == "nameserver")
                section = "dns_nameservers";
        }
        else if (key == "mode")
            mode = value;
        else if (key == "mixed-port")
            mixedPort = ParseInt(value, mixedPort);
        else if (key == "port")
            httpPort = ParseInt(value, httpPort);
        else if (key == "socks-port")
            socksPort = ParseInt(value, socksPort);
        else if (key == "allow-lan")
            allowLan = ParseBool(value, allowLan);
        else if (key == "ipv6")
            ipv6 = ParseBool(value, ipv6);
        else if (key == "log-level")
            logLevel = value;
        else if (key == "external-controller")
            externalController = value;
        else if (key == "secret")
            secret = value;
    }

    if (dnsNameservers.empty())
        dnsNameservers = {"223.5.5.5", "8.8.8.8"};
    return true;
}

bool MihomoConfig::Save(const std::string& path, std::string& error) const
{
    const wxFileName filename(wxString::FromUTF8(path));
    if (!wxFileName::Mkdir(filename.GetPath(), 0700, wxPATH_MKDIR_FULL) &&
        !wxDirExists(filename.GetPath()))
    {
        error = "Unable to create mihomo config directory";
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        error = "Unable to open mihomo config file for writing";
        return false;
    }

    file << "# WxClash mihomo configuration\n"
         << "mode: " << mode << "\n"
         << "mixed-port: " << mixedPort << "\n"
         << "port: " << httpPort << "\n"
         << "socks-port: " << socksPort << "\n"
         << "allow-lan: " << (allowLan ? "true" : "false") << "\n"
         << "ipv6: " << (ipv6 ? "true" : "false") << "\n"
         << "log-level: " << logLevel << "\n"
         << "external-controller: " << YamlString(externalController) << "\n"
         << "secret: " << YamlString(secret) << "\n"
         << "tun:\n"
         << "  enable: " << (tunEnable ? "true" : "false") << "\n"
         << "  stack: " << tunStack << "\n"
         << "dns:\n"
         << "  enable: " << (dnsEnable ? "true" : "false") << "\n"
         << "  enhanced-mode: " << dnsEnhancedMode << "\n"
         << "  nameserver:\n";
    for (const auto& nameserver : dnsNameservers)
        file << "    - " << YamlString(nameserver) << "\n";

    if (!file.good())
    {
        error = "Unable to write mihomo config file";
        return false;
    }
    return true;
}
