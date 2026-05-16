#include "FSCStickShaker/Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace fsc::stickshaker {
namespace {

std::string trim(std::string value)
{
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), isSpace));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), isSpace).base(), value.end());
    return value;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool parseBool(const std::string& value, bool fallback)
{
    const auto normalized = lower(trim(value));
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    return fallback;
}

int parseInt(const std::string& value, int fallback)
{
    try {
        std::size_t consumed = 0;
        const int parsed = std::stoi(trim(value), &consumed, 10);
        return consumed == trim(value).size() ? parsed : fallback;
    } catch (const std::exception&) {
        return fallback;
    }
}

std::vector<int> parseRelayChannels(const std::string& value, const std::vector<int>& fallback)
{
    std::vector<int> channels;
    std::stringstream input(value);
    std::string token;
    while (std::getline(input, token, ',')) {
        const int channel = parseInt(token, 0);
        if (channel >= 1 && channel <= 8) {
            channels.push_back(channel);
        }
    }
    return channels.empty() ? fallback : channels;
}

std::string joinRelayChannels(const std::vector<int>& channels)
{
    std::ostringstream output;
    for (std::size_t i = 0; i < channels.size(); ++i) {
        if (i != 0) {
            output << ',';
        }
        output << channels[i];
    }
    return output.str();
}

std::map<std::string, std::string> readKeyValues(const std::filesystem::path& path)
{
    std::map<std::string, std::string> values;
    std::ifstream input(path);
    if (!input) {
        return values;
    }

    std::string line;
    while (std::getline(input, line)) {
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) {
            line.erase(comment);
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        auto key = trim(line.substr(0, separator));
        auto value = trim(line.substr(separator + 1));
        if (!key.empty()) {
            values[std::move(key)] = std::move(value);
        }
    }

    return values;
}

std::string getString(const std::map<std::string, std::string>& values,
    const std::string& key,
    const std::string& fallback)
{
    const auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

} // namespace

std::string toString(TransportKind kind)
{
    switch (kind) {
    case TransportKind::Serial:
        return "serial";
    case TransportKind::Udp:
        return "udp";
    case TransportKind::Tcp:
        return "tcp";
    case TransportKind::LogOnly:
        return "log";
    }
    return "log";
}

TransportKind transportKindFromString(const std::string& value)
{
    const auto normalized = lower(trim(value));
    if (normalized == "serial") {
        return TransportKind::Serial;
    }
    if (normalized == "udp" || normalized == "tcp") {
        return TransportKind::Udp;
    }
    if (normalized == "tcp_stream") {
        return TransportKind::Tcp;
    }
    return TransportKind::LogOnly;
}

Config loadConfig(const std::filesystem::path& path)
{
    Config config;
    const auto values = readKeyValues(path);

    config.enabled = parseBool(getString(values, "shaker.enabled", "1"), config.enabled);
    config.debug = parseBool(getString(values, "shaker.debug", "0"), config.debug);
    config.transport = transportKindFromString(getString(values, "shaker.transport", toString(config.transport)));

    config.serial.port = getString(values, "shaker.serial.port", config.serial.port);
    config.serial.baud = parseInt(getString(values, "shaker.serial.baud", ""), config.serial.baud);
    config.serial.dataBits = parseInt(getString(values, "shaker.serial.data_bits", ""), config.serial.dataBits);
    config.serial.parity = lower(getString(values, "shaker.serial.parity", config.serial.parity));
    config.serial.stopBits = parseInt(getString(values, "shaker.serial.stop_bits", ""), config.serial.stopBits);

    config.udp.ip = getString(values, "shaker.udp.ip", getString(values, "shaker.tcp.ip", config.udp.ip));
    config.udp.sourcePort = parseInt(getString(values, "shaker.udp.source_port", ""), config.udp.sourcePort);
    config.udp.destinationPort = parseInt(
        getString(values, "shaker.udp.destination_port", getString(values, "shaker.tcp.port", "")),
        config.udp.destinationPort);
    config.udp.relayChannels = parseRelayChannels(
        getString(values, "shaker.udp.relay_channels", ""),
        config.udp.relayChannels);

    config.tcp.ip = getString(values, "shaker.tcp.ip", config.tcp.ip);
    config.tcp.port = parseInt(getString(values, "shaker.tcp.port", ""), config.tcp.port);

    return config;
}

void saveDefaultConfig(const std::filesystem::path& path, const Config& config)
{
    std::filesystem::create_directories(path.parent_path());

    std::ofstream output(path);
    output << "# FSC Stick Shaker preferences\n"
           << "# Transport is one of: log, serial, udp\n"
           << "shaker.enabled=" << (config.enabled ? 1 : 0) << '\n'
           << "shaker.transport=" << toString(config.transport) << '\n'
           << "shaker.serial.port=" << config.serial.port << '\n'
           << "shaker.serial.baud=" << config.serial.baud << '\n'
           << "shaker.serial.data_bits=" << config.serial.dataBits << '\n'
           << "shaker.serial.parity=" << config.serial.parity << '\n'
           << "shaker.serial.stop_bits=" << config.serial.stopBits << '\n'
           << "shaker.udp.ip=" << config.udp.ip << '\n'
           << "shaker.udp.source_port=" << config.udp.sourcePort << '\n'
           << "shaker.udp.destination_port=" << config.udp.destinationPort << '\n'
           << "shaker.udp.relay_channels=" << joinRelayChannels(config.udp.relayChannels) << '\n'
           << "shaker.tcp.ip=" << config.tcp.ip << '\n'
           << "shaker.tcp.port=" << config.tcp.port << '\n'
           << "shaker.debug=" << (config.debug ? 1 : 0) << '\n';
}

} // namespace fsc::stickshaker
