#pragma once

#include <filesystem>
#include <string>

namespace fsc::stickshaker {

enum class TransportKind {
    Serial,
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

struct Config {
    bool enabled = true;
    bool debug = false;
    TransportKind transport = TransportKind::Tcp;
    SerialConfig serial;
    TcpConfig tcp;
};

Config loadConfig(const std::filesystem::path& path);
void saveDefaultConfig(const std::filesystem::path& path, const Config& config = {});

std::string toString(TransportKind kind);
TransportKind transportKindFromString(const std::string& value);

} // namespace fsc::stickshaker
