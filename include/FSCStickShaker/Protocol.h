#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fsc::stickshaker {

struct ShakerState {
    bool captain = false;
    bool firstOfficer = false;

    bool any() const
    {
        return captain || firstOfficer;
    }

    bool operator==(const ShakerState& other) const
    {
        return captain == other.captain && firstOfficer == other.firstOfficer;
    }
};

std::vector<std::uint8_t> serialFrame(bool active);
std::vector<std::string> asciiRelayFrames(ShakerState state, const std::vector<int>& channels);
std::vector<std::string> asciiRelayFrames(bool active, const std::vector<int>& channels);
std::vector<std::vector<std::uint8_t>> relayFrames(ShakerState state);
std::vector<std::vector<std::uint8_t>> relayFrames(bool active);
std::vector<std::string> tcpFrames(ShakerState state);
std::vector<std::string> tcpFrames(bool active);
std::string bytesToHex(const std::vector<std::uint8_t>& bytes);

} // namespace fsc::stickshaker
