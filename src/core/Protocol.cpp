#include "FSCStickShaker/Protocol.h"

#include <iomanip>
#include <sstream>

namespace fsc::stickshaker {

std::vector<std::uint8_t> serialFrame(bool active)
{
    return {0xFF, 0x01, static_cast<std::uint8_t>(active ? 0x01 : 0x00)};
}

std::vector<std::string> asciiRelayFrames(ShakerState state, const std::vector<int>& channels)
{
    std::vector<std::string> frames;
    frames.reserve(channels.size());
    for (std::size_t index = 0; index < channels.size(); ++index) {
        const int channel = channels[index];
        if (channel < 1 || channel > 99) {
            continue;
        }

        bool active = state.any();
        if (index == 0) {
            active = state.captain;
        } else if (index == 1) {
            active = state.firstOfficer;
        }

        std::ostringstream frame;
        frame << "FF" << std::setw(2) << std::setfill('0') << channel << '0' << (active ? '1' : '0');
        frames.push_back(frame.str());
    }
    return frames;
}

std::vector<std::string> asciiRelayFrames(bool active, const std::vector<int>& channels)
{
    return asciiRelayFrames({active, active}, channels);
}

std::vector<std::vector<std::uint8_t>> relayFrames(ShakerState state)
{
    return {
        {0xFF, 0x01, static_cast<std::uint8_t>(state.captain ? 0x01 : 0x00)},
        {0xFF, 0x02, static_cast<std::uint8_t>(state.firstOfficer ? 0x01 : 0x00)},
    };
}

std::vector<std::vector<std::uint8_t>> relayFrames(bool active)
{
    return relayFrames({active, active});
}

std::vector<std::string> tcpFrames(ShakerState state)
{
    return asciiRelayFrames(state, {1, 2});
}

std::vector<std::string> tcpFrames(bool active)
{
    return tcpFrames({active, active});
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
