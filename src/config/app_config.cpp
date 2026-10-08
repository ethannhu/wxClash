#include "app_config.h"

#include <wx/filename.h>

#include <yaml-cpp/yaml.h>

#include <cctype>
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

    void ReadConfigValues(const YAML::Node& root, MihomoConfig& config)
    {
        ReadScalar(root, "mode", config.mode);
        ReadScalar(root, "mixed-port", config.mixedPort);
        ReadScalar(root, "port", config.httpPort);
        ReadScalar(root, "socks-port", config.socksPort);
        ReadScalar(root, "allow-lan", config.allowLan);
        ReadScalar(root, "ipv6", config.ipv6);
        ReadScalar(root, "log-level", config.logLevel);
        ReadScalar(root, "external-controller", config.externalController);
        ReadScalar(root, "secret", config.secret);

        const auto tun = root["tun"];
        if (tun)
        {
            if (!tun.IsMap())
                throw std::runtime_error("'tun' must be a map");
            ReadScalar(tun, "enable", config.tunEnable);
            ReadScalar(tun, "stack", config.tunStack);
        }

        const auto dns = root["dns"];
        if (dns)
        {
            if (!dns.IsMap())
                throw std::runtime_error("'dns' must be a map");
            ReadScalar(dns, "enable", config.dnsEnable);
            ReadScalar(dns, "enhanced-mode", config.dnsEnhancedMode);

            const auto nameservers = dns["nameserver"];
            if (nameservers)
            {
                if (!nameservers.IsSequence())
                    throw std::runtime_error("'dns.nameserver' must be a sequence");
                config.dnsNameservers.clear();
                for (const auto& nameserver : nameservers)
                    config.dnsNameservers.push_back(nameserver.as<std::string>());
            }
        }
    }

    void ValidatePorts(const MihomoConfig& config)
    {
        if (config.mixedPort < 0 || config.mixedPort > 65535 ||
            config.httpPort < 0 || config.httpPort > 65535 ||
            config.socksPort < 0 || config.socksPort > 65535)
        {
            throw std::runtime_error("port values must be between 0 and 65535");
        }
    }

    bool ValidateConfig(const MihomoConfig& config, std::string& error)
    {
        return ValidateExternalController(config.externalController, error);
    }

    bool ParseConfigFile(const std::string& path, MihomoConfig& config,
                         std::string& error, const char* description,
                         YAML::Node* sourceOut)
    {
        try
        {
            const auto root = YAML::LoadFile(path);
            if (!root.IsMap())
                throw std::runtime_error("top-level YAML value must be a map");

            if (sourceOut)
            {
                *sourceOut = YAML::Clone(root);
                config.dnsNameservers.clear();
            }
            ReadConfigValues(root, config);
            if (!ValidateConfig(config, error))
                return false;
            ValidatePorts(config);
            if (config.dnsNameservers.empty())
                config.dnsNameservers = {"223.5.5.5", "8.8.8.8"};
            return true;
        }
        catch (const YAML::Exception& exception)
        {
            error = "Unable to parse " + std::string(description) + ": " +
                    exception.what();
        }
        catch (const std::exception& exception)
        {
            error = "Invalid " + std::string(description) + ": " +
                    exception.what();
        }
        return false;
    }

    void WriteManagedFields(YAML::Node& root, const MihomoConfig& config)
    {
        root["mode"] = config.mode;
        root["mixed-port"] = config.mixedPort;
        root["port"] = config.httpPort;
        root["socks-port"] = config.socksPort;
        root["allow-lan"] = config.allowLan;
        root["ipv6"] = config.ipv6;
        root["log-level"] = config.logLevel;
        root["external-controller"] = config.externalController;
        root["secret"] = config.secret;

        YAML::Node tun(YAML::NodeType::Map);
        tun["enable"] = config.tunEnable;
        tun["stack"] = config.tunStack;
        root["tun"] = tun;

        YAML::Node dns(YAML::NodeType::Map);
        dns["enable"] = config.dnsEnable;
        dns["enhanced-mode"] = config.dnsEnhancedMode;
        YAML::Node nameservers(YAML::NodeType::Sequence);
        for (const auto& nameserver : config.dnsNameservers)
            nameservers.push_back(nameserver);
        dns["nameserver"] = nameservers;
        root["dns"] = dns;
    }

    bool EnsureParentDirectory(const std::string& path, const char* description,
                               std::string& error)
    {
        const wxFileName filename(wxString::FromUTF8(path));
        if (!wxFileName::Mkdir(filename.GetPath(), 0700, wxPATH_MKDIR_FULL) &&
            !wxDirExists(filename.GetPath()))
        {
            error = "Unable to create mihomo " + std::string(description) +
                    " directory";
            return false;
        }
        return true;
    }

    bool WriteYamlFile(const std::string& path, const YAML::Node& root,
                       const char* description, const char* comment,
                       std::string& error)
    {
        YAML::Emitter emitter;
        emitter << root;
        if (!emitter.good())
        {
            error = "Unable to serialize mihomo " + std::string(description);
            return false;
        }

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            error = "Unable to open mihomo " + std::string(description) +
                    " file for writing";
            return false;
        }

        file << comment << "\n\n" << emitter.c_str() << "\n";
        if (!file.good())
        {
            error = "Unable to write mihomo " + std::string(description) +
                    " file";
            return false;
        }
        return true;
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
    catch (const std::exception&)
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

    MihomoConfig parsed = *this;
    YAML::Node source;
    if (!ParseConfigFile(path, parsed, error, "mihomo config", &source))
        return false;
    parsed.sourceConfig_ = std::move(source);
    *this = std::move(parsed);
    return true;
}

bool MihomoConfig::LoadOverrides(const std::string& path, std::string& error)
{
    if (!wxFileName::FileExists(wxString::FromUTF8(path)))
        return true;

    MihomoConfig parsed = *this;
    if (!ParseConfigFile(path, parsed, error, "mihomo settings", nullptr))
        return false;
    *this = std::move(parsed);
    return true;
}

bool MihomoConfig::SaveOverrides(const std::string& path,
                                 std::string& error) const
{
    if (!ValidateExternalController(externalController, error))
        return false;
    if (!EnsureParentDirectory(path, "settings", error))
        return false;

    YAML::Node root(YAML::NodeType::Map);
    WriteManagedFields(root, *this);
    return WriteYamlFile(path, root, "settings",
                         "# WxClash Mihomo page settings", error);
}

bool MihomoConfig::Save(const std::string& path, std::string& error) const
{
    if (!ValidateExternalController(externalController, error))
        return false;
    if (!EnsureParentDirectory(path, "config", error))
        return false;

    YAML::Node root = sourceConfig_ && sourceConfig_.IsMap()
                          ? YAML::Clone(sourceConfig_)
                          : YAML::Node(YAML::NodeType::Map);
    WriteManagedFields(root, *this);
    return WriteYamlFile(path, root, "config",
                         "# WxClash mihomo configuration", error);
}
