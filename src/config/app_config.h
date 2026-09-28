#pragma once

#include <string>
#include <vector>

struct MihomoConfig
{
    std::string mode = "rule";
    int mixedPort = 7897;
    int httpPort = 7899;
    int socksPort = 7898;
    bool allowLan = false;
    bool ipv6 = true;
    std::string logLevel = "info";
    std::string externalController = "127.0.0.1:9097";
    std::string secret;

    bool tunEnable = false;
    std::string tunStack = "gvisor";

    bool dnsEnable = false;
    std::string dnsEnhancedMode = "fake-ip";
    std::vector<std::string> dnsNameservers = {"223.5.5.5", "8.8.8.8"};

    bool Load(const std::string& path, std::string& error);
    bool Save(const std::string& path, std::string& error) const;
};
