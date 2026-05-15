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

std::vector<std::string> parseList(const std::string& value, std::vector<std::string> fallback)
{
    if (trim(value).empty()) {
        return fallback;
    }

    std::vector<std::string> result;
    std::stringstream input(value);
    std::string item;
    while (std::getline(input, item, ',')) {
        item = trim(item);
        if (!item.empty()) {
            result.push_back(std::move(item));
        }
    }
    return result.empty() ? std::move(fallback) : result;
}

std::string joinList(const std::vector<std::string>& values)
{
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out += ',';
        }
        out += values[i];
    }
    return out;
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
    if (normalized == "tcp") {
        return TransportKind::Tcp;
    }
    return TransportKind::LogOnly;
}

Config loadConfig(const std::filesystem::path& path)
{
    Config config;
    const auto values = readKeyValues(path);

    config.enabled = parseBool(getString(values, "shaker.enabled", "0"), config.enabled);
    config.debug = parseBool(getString(values, "shaker.debug", "0"), config.debug);
    config.transport = transportKindFromString(getString(values, "shaker.transport", toString(config.transport)));
    config.source = getString(values, "shaker.source", config.source);
    config.aircraft.tailnums = parseList(getString(values, "shaker.aircraft.tailnums", ""), config.aircraft.tailnums);
    config.aircraft.requireZiboPlugin =
        parseBool(getString(values, "shaker.aircraft.require_zibo_plugin", ""), config.aircraft.requireZiboPlugin);
    config.aircraft.deferUntilDatarefs =
        parseBool(getString(values, "shaker.defer_until_datarefs", ""), config.aircraft.deferUntilDatarefs);
    config.aircraft.retryIntervalSec =
        parseInt(getString(values, "shaker.retry_interval_sec", ""), config.aircraft.retryIntervalSec);

    config.serial.port = getString(values, "shaker.serial.port", config.serial.port);
    config.serial.baud = parseInt(getString(values, "shaker.serial.baud", ""), config.serial.baud);
    config.serial.dataBits = parseInt(getString(values, "shaker.serial.data_bits", ""), config.serial.dataBits);
    config.serial.parity = getString(values, "shaker.serial.parity", config.serial.parity);
    config.serial.stopBits = parseInt(getString(values, "shaker.serial.stop_bits", ""), config.serial.stopBits);
    config.serial.dtr = parseBool(getString(values, "shaker.serial.dtr", ""), config.serial.dtr);
    config.serial.rts = parseBool(getString(values, "shaker.serial.rts", ""), config.serial.rts);
    config.serial.xonxoff = parseBool(getString(values, "shaker.serial.xonxoff", ""), config.serial.xonxoff);

    config.tcp.ip = getString(values, "shaker.tcp.ip", config.tcp.ip);
    config.tcp.port = parseInt(getString(values, "shaker.tcp.port", ""), config.tcp.port);

    return config;
}

void saveDefaultConfig(const std::filesystem::path& path, const Config& config)
{
    std::filesystem::create_directories(path.parent_path());

    std::ofstream output(path);
    output << "# FSC Stick Shaker preferences\n"
           << "# Transport is one of: log, serial, tcp\n"
           << "shaker.enabled=" << (config.enabled ? 1 : 0) << '\n'
           << "shaker.transport=" << toString(config.transport) << '\n'
           << "shaker.source=" << config.source << '\n'
           << "shaker.aircraft.tailnums=" << joinList(config.aircraft.tailnums) << '\n'
           << "shaker.aircraft.require_zibo_plugin=" << (config.aircraft.requireZiboPlugin ? 1 : 0) << '\n'
           << "shaker.defer_until_datarefs=" << (config.aircraft.deferUntilDatarefs ? 1 : 0) << '\n'
           << "shaker.retry_interval_sec=" << config.aircraft.retryIntervalSec << '\n'
           << "shaker.serial.port=" << config.serial.port << '\n'
           << "shaker.serial.baud=" << config.serial.baud << '\n'
           << "shaker.serial.data_bits=" << config.serial.dataBits << '\n'
           << "shaker.serial.parity=" << config.serial.parity << '\n'
           << "shaker.serial.stop_bits=" << config.serial.stopBits << '\n'
           << "shaker.serial.dtr=" << (config.serial.dtr ? 1 : 0) << '\n'
           << "shaker.serial.rts=" << (config.serial.rts ? 1 : 0) << '\n'
           << "shaker.serial.xonxoff=" << (config.serial.xonxoff ? 1 : 0) << '\n'
           << "shaker.tcp.ip=" << config.tcp.ip << '\n'
           << "shaker.tcp.port=" << config.tcp.port << '\n'
           << "shaker.debug=" << (config.debug ? 1 : 0) << '\n';
}

} // namespace fsc::stickshaker
