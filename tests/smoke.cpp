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
    assert((tcpFrames(false) == std::vector<std::string> {"FF0100", "FF0200"}));
    assert((tcpFrames(true) == std::vector<std::string> {"FF0101", "FF0201"}));

    const auto path = std::filesystem::temp_directory_path() / "FSCStickShaker-smoke.prf";
    saveDefaultConfig(path);

    auto config = loadConfig(path);
    assert(config.enabled == false);
    assert(config.transport == TransportKind::LogOnly);
    assert((config.aircraft.tailnums == std::vector<std::string> {"ZB738", "B738"}));
    assert(config.aircraft.requireZiboPlugin == true);
    assert(config.aircraft.deferUntilDatarefs == true);
    assert(config.serial.baud == 115200);

    config.enabled = true;
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
