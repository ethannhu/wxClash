#include "app_config.h"

#include <wx/filename.h>

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <cctype>
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

}

bool ValidateExternalController(const std::string& value, std::string& error)
{
    const std::string& address = value;

    if (address.empty() || address.find('/') != std::string::npos ||
        address.find_first_of(" \t\r\n") != std::string::npos)
    {
        error = "External controller must be host:port";
        return false;
    }

    std::size_t portSeparator = std::string::npos;
    if (address.front() == '[')
    {
        const auto closingBracket = address.find(']');
        if (closingBracket == std::string::npos ||
            closingBracket + 1 >= address.size() ||
            address[closingBracket + 1] != ':')
        {
            error = "External controller IPv6 address must be [host]:port";
            return false;
        }
        portSeparator = closingBracket + 1;
    }
    else
    {
        portSeparator = address.rfind(':');
        if (portSeparator == std::string::npos ||
            address.find(':') != portSeparator)
        {
            error = "External controller must be host:port";
            return false;
        }
    }

    const auto host = address.substr(0, portSeparator);
    const auto port = address.substr(portSeparator + 1);
    if (host.empty() || port.empty())
    {
        error = "External controller must include both host and port";
        return false;
    }
    for (const auto character : port)
    {
        if (!std::isdigit(static_cast<unsigned char>(character)))
        {
            error = "External controller port must be numeric";
            return false;
        }
    }
    try
    {
        const auto portNumber = std::stoul(port);
        if (portNumber == 0 || portNumber > 65535)
        {
            error = "External controller port must be between 1 and 65535";
            return false;
        }
    }
    catch (...)
    {
        error = "External controller port is invalid";
        return false;
    }
    return true;
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
        parsed.sourceConfig_ = root;
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

        if (!ValidateExternalController(parsed.externalController, error))
            return false;

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

bool MihomoConfig::LoadOverrides(const std::string& path, std::string& error)
{
    if (!wxFileName::FileExists(wxString::FromUTF8(path)))
        return true;
    try
    {
        const auto root = YAML::LoadFile(path);
        if (!root.IsMap())
            throw std::runtime_error("top-level YAML value must be a map");
        MihomoConfig parsed = *this;
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
            if (!tun.IsMap()) throw std::runtime_error("'tun' must be a map");
            ReadScalar(tun, "enable", parsed.tunEnable);
            ReadScalar(tun, "stack", parsed.tunStack);
        }
        const auto dns = root["dns"];
        if (dns)
        {
            if (!dns.IsMap()) throw std::runtime_error("'dns' must be a map");
            ReadScalar(dns, "enable", parsed.dnsEnable);
            ReadScalar(dns, "enhanced-mode", parsed.dnsEnhancedMode);
            const auto nameservers = dns["nameserver"];
            if (nameservers)
            {
                if (!nameservers.IsSequence())
                    throw std::runtime_error("'dns.nameserver' must be a sequence");
                parsed.dnsNameservers.clear();
                for (const auto& nameserver : nameservers)
                    parsed.dnsNameservers.push_back(nameserver.as<std::string>());
            }
        }
        if (!ValidateExternalController(parsed.externalController, error)) return false;
        if (parsed.mixedPort < 0 || parsed.mixedPort > 65535 ||
            parsed.httpPort < 0 || parsed.httpPort > 65535 ||
            parsed.socksPort < 0 || parsed.socksPort > 65535)
            throw std::runtime_error("port values must be between 0 and 65535");
        if (parsed.dnsNameservers.empty())
            parsed.dnsNameservers = {"223.5.5.5", "8.8.8.8"};
        *this = std::move(parsed);
        return true;
    }
    catch (const YAML::Exception& exception)
    {
        error = "Unable to parse mihomo settings: " + std::string(exception.what());
    }
    catch (const std::exception& exception)
    {
        error = "Invalid mihomo settings: " + std::string(exception.what());
    }
    return false;
}

bool MihomoConfig::SaveOverrides(const std::string& path, std::string& error) const
{
    if (!ValidateExternalController(externalController, error)) return false;
    const wxFileName filename(wxString::FromUTF8(path));
    if (!wxFileName::Mkdir(filename.GetPath(), 0700, wxPATH_MKDIR_FULL) &&
        !wxDirExists(filename.GetPath()))
    {
        error = "Unable to create mihomo settings directory";
        return false;
    }
    YAML::Node root(YAML::NodeType::Map);
    root["mode"] = mode; root["mixed-port"] = mixedPort; root["port"] = httpPort;
    root["socks-port"] = socksPort; root["allow-lan"] = allowLan; root["ipv6"] = ipv6;
    root["log-level"] = logLevel; root["external-controller"] = externalController;
    root["secret"] = secret;
    YAML::Node tun(YAML::NodeType::Map);
    tun["enable"] = tunEnable; tun["stack"] = tunStack; root["tun"] = tun;
    YAML::Node dns(YAML::NodeType::Map);
    dns["enable"] = dnsEnable; dns["enhanced-mode"] = dnsEnhancedMode;
    YAML::Node nameservers(YAML::NodeType::Sequence);
    for (const auto& nameserver : dnsNameservers) nameservers.push_back(nameserver);
    dns["nameserver"] = nameservers; root["dns"] = dns;
    YAML::Emitter emitter; emitter << root;
    if (!emitter.good()) { error = "Unable to serialize mihomo settings"; return false; }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) { error = "Unable to open mihomo settings file for writing"; return false; }
    file << "# WxClash Mihomo page settings\n\n" << emitter.c_str() << "\n";
    if (!file.good()) { error = "Unable to write mihomo settings file"; return false; }
    return true;
}

bool MihomoConfig::Save(const std::string& path, std::string& error) const
{
    if (!ValidateExternalController(externalController, error))
        return false;

    const wxFileName filename(wxString::FromUTF8(path));
    if (!wxFileName::Mkdir(filename.GetPath(), 0700, wxPATH_MKDIR_FULL) &&
        !wxDirExists(filename.GetPath()))
    {
        error = "Unable to create mihomo config directory";
        return false;
    }

    YAML::Node root = sourceConfig_;
    if (!root || !root.IsMap())
        root = YAML::Node(YAML::NodeType::Map);

    root["mode"] = mode;
    root["mixed-port"] = mixedPort;
    root["port"] = httpPort;
    root["socks-port"] = socksPort;
    root["allow-lan"] = allowLan;
    root["ipv6"] = ipv6;
    root["log-level"] = logLevel;
    root["external-controller"] = externalController;
    root["secret"] = secret;

    YAML::Node tun = root["tun"];
    if (!tun || !tun.IsMap())
        tun = YAML::Node(YAML::NodeType::Map);
    tun["enable"] = tunEnable;
    tun["stack"] = tunStack;
    root["tun"] = tun;

    YAML::Node dns = root["dns"];
    if (!dns || !dns.IsMap())
        dns = YAML::Node(YAML::NodeType::Map);
    dns["enable"] = dnsEnable;
    dns["enhanced-mode"] = dnsEnhancedMode;
    YAML::Node nameservers(YAML::NodeType::Sequence);
    for (const auto& nameserver : dnsNameservers)
        nameservers.push_back(nameserver);
    dns["nameserver"] = nameservers;
    root["dns"] = dns;

    YAML::Emitter emitter;
    emitter << root;
    if (!emitter.good())
    {
        error = "Unable to serialize mihomo config";
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        error = "Unable to open mihomo config file for writing";
        return false;
    }

    file << "# WxClash mihomo configuration\n\n" << emitter.c_str() << "\n";

    if (!file.good())
    {
        error = "Unable to write mihomo config file";
        return false;
    }
    return true;
}
