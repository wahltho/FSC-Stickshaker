#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fsc::stickshaker {

std::vector<std::uint8_t> serialFrame(bool active);
std::vector<std::string> asciiRelayFrames(bool active, const std::vector<int>& channels);
std::vector<std::vector<std::uint8_t>> relayFrames(bool active);
std::vector<std::string> tcpFrames(bool active);
std::string bytesToHex(const std::vector<std::uint8_t>& bytes);

} // namespace fsc::stickshaker
