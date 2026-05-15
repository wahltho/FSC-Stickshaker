#pragma once

#include <filesystem>
#include <string>
#include <vector>

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
    bool dtr = true;
    bool rts = true;
    bool xonxoff = false;
};

struct TcpConfig {
    std::string ip = "192.168.0.10";
    int port = 12345;
};

struct AircraftConfig {
    std::vector<std::string> tailnums = {"ZB738", "B738"};
    bool requireZiboPlugin = true;
    bool deferUntilDatarefs = true;
    int retryIntervalSec = 5;
};

struct Config {
    bool enabled = false;
    bool debug = false;
    TransportKind transport = TransportKind::LogOnly;
    std::string source = "stall_warning";
    AircraftConfig aircraft;
    SerialConfig serial;
    TcpConfig tcp;
};

Config loadConfig(const std::filesystem::path& path);
void saveDefaultConfig(const std::filesystem::path& path, const Config& config = {});

std::string toString(TransportKind kind);
TransportKind transportKindFromString(const std::string& value);

} // namespace fsc::stickshaker
