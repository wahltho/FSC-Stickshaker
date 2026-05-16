#include "FSCStickShaker/Protocol.h"

#include <iomanip>
#include <sstream>

namespace fsc::stickshaker {

std::vector<std::uint8_t> serialFrame(bool active)
{
    return {0xFF, 0x01, static_cast<std::uint8_t>(active ? 0x01 : 0x00)};
}

std::vector<std::string> asciiRelayFrames(bool active, const std::vector<int>& channels)
{
    const char state = active ? '1' : '0';
    std::vector<std::string> frames;
    frames.reserve(channels.size());
    for (const int channel : channels) {
        if (channel < 1 || channel > 99) {
            continue;
        }
        std::ostringstream frame;
        frame << "FF" << std::setw(2) << std::setfill('0') << channel << '0' << state;
        frames.push_back(frame.str());
    }
    return frames;
}

std::vector<std::vector<std::uint8_t>> relayFrames(bool active)
{
    const auto state = static_cast<std::uint8_t>(active ? 0x01 : 0x00);
    return {
        {0xFF, 0x01, state},
        {0xFF, 0x02, state},
    };
}

std::vector<std::string> tcpFrames(bool active)
{
    const char state = active ? '1' : '0';
    return {
        std::string("FF010") + state,
        std::string("FF020") + state,
    };
}

std::string bytesToHex(const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0) {
            out << ' ';
        }
        out << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return out.str();
}

} // namespace fsc::stickshaker
