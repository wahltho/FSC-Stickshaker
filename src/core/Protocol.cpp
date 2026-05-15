#include "FSCStickShaker/Protocol.h"

#include <iomanip>
#include <sstream>

namespace fsc::stickshaker {

std::vector<std::uint8_t> serialFrame(bool active)
{
    return {0xFF, 0x01, static_cast<std::uint8_t>(active ? 0x01 : 0x00)};
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
