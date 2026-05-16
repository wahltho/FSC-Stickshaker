#include "FSCStickShaker/Config.h"
#include "FSCStickShaker/Protocol.h"
#include "FSCStickShaker/ShakerController.h"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

int main()
{
    using namespace fsc::stickshaker;

    assert((serialFrame(false) == std::vector<unsigned char> {0xFF, 0x01, 0x00}));
    assert((serialFrame(true) == std::vector<unsigned char> {0xFF, 0x01, 0x01}));
    assert((asciiRelayFrames(false, {5}) == std::vector<std::string> {"FF0500"}));
    assert((asciiRelayFrames(true, {5}) == std::vector<std::string> {"FF0501"}));
    assert((relayFrames(false) == std::vector<std::vector<unsigned char>> {{0xFF, 0x01, 0x00}, {0xFF, 0x02, 0x00}}));
    assert((relayFrames(true) == std::vector<std::vector<unsigned char>> {{0xFF, 0x01, 0x01}, {0xFF, 0x02, 0x01}}));
    assert((tcpFrames(false) == std::vector<std::string> {"FF0100", "FF0200"}));
    assert((tcpFrames(true) == std::vector<std::string> {"FF0101", "FF0201"}));

    const auto path = std::filesystem::temp_directory_path() / "FSCStickShaker-smoke.prf";
    saveDefaultConfig(path);

    auto config = loadConfig(path);
    assert(config.enabled == true);
    assert(config.transport == TransportKind::Udp);
    assert(transportKindFromString("tcp") == TransportKind::Udp);
    assert(config.udp.ip == "192.168.1.199");
    assert(config.udp.destinationPort == 12345);
    assert((config.udp.relayChannels == std::vector<int> {5}));
    assert(config.serial.baud == 115200);

    config.transport = TransportKind::LogOnly;
    ShakerController controller;
    controller.configure(config);
    controller.updateTrigger(false);
    controller.updateTrigger(true);
    controller.updateTrigger(true);
    controller.forceOff();
    controller.shutdown();

    std::filesystem::remove(path);
    return 0;
}
