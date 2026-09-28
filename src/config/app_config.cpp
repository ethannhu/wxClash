#include "app_config.h"

#include <wx/filename.h>

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <stdexcept>
#include <utility>

namespace
{
    template <typename T>
    void ReadScalar(const YAML::Node& parent, const char* key, T& target)
    {
        const auto value = parent[key];
        if (value)
            target = value.as<T>();
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
    if (!wxFileName::FileExists(wxString::FromUTF8(path)))
        return true;

    try
    {
        const auto root = YAML::LoadFile(path);
        if (!root.IsMap())
            throw std::runtime_error("top-level YAML value must be a map");

        // Parse into a copy so a malformed file cannot leave the live
        // configuration partially updated.
        MihomoConfig parsed = *this;
        parsed.dnsNameservers.clear();

        ReadScalar(root, "mode", parsed.mode);
        ReadScalar(root, "mixed-port", parsed.mixedPort);
        ReadScalar(root, "port", parsed.httpPort);
        ReadScalar(root, "socks-port", parsed.socksPort);
        ReadScalar(root, "allow-lan", parsed.allowLan);
        ReadScalar(root, "ipv6", parsed.ipv6);
        ReadScalar(root, "log-level", parsed.logLevel);
        ReadScalar(root, "external-controller", parsed.externalController);
        ReadScalar(root, "secret", parsed.secret);

        const auto tun = root["tun"];
        if (tun)
        {
            if (!tun.IsMap())
                throw std::runtime_error("'tun' must be a map");
            ReadScalar(tun, "enable", parsed.tunEnable);
            ReadScalar(tun, "stack", parsed.tunStack);
        }

        const auto dns = root["dns"];
        if (dns)
        {
            if (!dns.IsMap())
                throw std::runtime_error("'dns' must be a map");
            ReadScalar(dns, "enable", parsed.dnsEnable);
            ReadScalar(dns, "enhanced-mode", parsed.dnsEnhancedMode);

            const auto nameservers = dns["nameserver"];
            if (nameservers)
            {
                if (!nameservers.IsSequence())
                    throw std::runtime_error("'dns.nameserver' must be a sequence");
                for (const auto& nameserver : nameservers)
                    parsed.dnsNameservers.push_back(nameserver.as<std::string>());
            }
        }

        if (parsed.mixedPort < 0 || parsed.mixedPort > 65535 ||
            parsed.httpPort < 0 || parsed.httpPort > 65535 ||
            parsed.socksPort < 0 || parsed.socksPort > 65535)
        {
            throw std::runtime_error("port values must be between 0 and 65535");
        }
        if (parsed.dnsNameservers.empty())
            parsed.dnsNameservers = {"223.5.5.5", "8.8.8.8"};

        *this = std::move(parsed);
        return true;
    }
    catch (const YAML::Exception& exception)
    {
        error = "Unable to parse mihomo config: " + std::string(exception.what());
    }
    catch (const std::exception& exception)
    {
        error = "Invalid mihomo config: " + std::string(exception.what());
    }
    return false;
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
