#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fsc::stickshaker {

enum class TransportKind {
    Serial,
    Udp,
    Tcp,
    LogOnly,
};

struct SerialConfig {
    std::string port;
    int baud = 115200;
    int dataBits = 8;
    std::string parity = "none";
    int stopBits = 1;
};

struct TcpConfig {
    std::string ip = "192.168.1.199";
    int port = 12345;
};

struct UdpConfig {
    std::string ip = "192.168.1.199";
    int sourcePort = 12345;
    int destinationPort = 12345;
    std::vector<int> relayChannels {5};
};

struct Config {
    bool enabled = true;
    bool debug = false;
    TransportKind transport = TransportKind::Udp;
    SerialConfig serial;
    UdpConfig udp;
    TcpConfig tcp;
};

Config loadConfig(const std::filesystem::path& path);
void saveDefaultConfig(const std::filesystem::path& path, const Config& config = {});

std::string toString(TransportKind kind);
TransportKind transportKindFromString(const std::string& value);

} // namespace fsc::stickshaker
