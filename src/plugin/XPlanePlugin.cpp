#include "FSCStickShaker/Config.h"
#include "FSCStickShaker/Log.h"
#include "FSCStickShaker/ShakerController.h"

#include "XPLMDataAccess.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

namespace {

using fsc::stickshaker::Config;
using fsc::stickshaker::ShakerController;

constexpr const char* kPluginName = "FSC Stick Shaker";
constexpr const char* kPluginSignature = "de.wahltho.fsc.stickshaker";
constexpr const char* kPluginDescription = "Standalone FSC Stick Shaker driver";
constexpr const char* kPrefsFileName = "FSCStickShaker.prf";

ShakerController gController;
std::filesystem::path gPrefsPath;
XPLMDataRef gTriggerDataRef = nullptr;
XPLMCommandRef gReloadPrefsCommand = nullptr;
XPLMCommandRef gTestOnCommand = nullptr;
XPLMCommandRef gTestOffCommand = nullptr;
XPLMCommandRef gTestPulseCommand = nullptr;
XPLMDataRef gTailnumDataRef = nullptr;
bool gAutoTriggerReady = false;
std::chrono::steady_clock::time_point gNextResolveAttempt {};

const char* sourceDataRefPath(const std::string& source)
{
    if (source == "stall_warning_ratio") {
        return "sim/cockpit2/annunciators/stall_warning_ratio";
    }
    if (source == "zibo_stall_1g") {
        return "laminar/B738/system/stall_1g";
    }
    return "sim/cockpit2/annunciators/stall_warning";
}

bool sourceReadsAsFloat(const std::string& source)
{
    return source == "stall_warning_ratio" || source == "zibo_stall_1g";
}

std::string trim(std::string value)
{
    const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), isSpace));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), isSpace).base(), value.end());
    return value;
}

std::filesystem::path prefsPath()
{
    std::array<char, 1024> buffer {};
    XPLMGetPrefsPath(buffer.data());

    std::filesystem::path path(buffer.data());
    if (path.filename().string().find(".prf") != std::string::npos) {
        path = path.parent_path();
    }
    return path / kPrefsFileName;
}

void xplaneLog(const std::string& message)
{
    XPLMDebugString(("[FSCStickShaker] " + message + "\n").c_str());
}

std::string readTailnum()
{
    if (!gTailnumDataRef) {
        gTailnumDataRef = XPLMFindDataRef("sim/aircraft/view/acf_tailnum");
    }
    if (!gTailnumDataRef) {
        return {};
    }

    std::array<char, 256> buffer {};
    const int bytes = XPLMGetDatab(gTailnumDataRef, buffer.data(), 0, static_cast<int>(buffer.size() - 1));
    if (bytes <= 0) {
        return {};
    }

    std::string tail(buffer.data(), static_cast<std::size_t>(bytes));
    const auto nul = tail.find('\0');
    if (nul != std::string::npos) {
        tail.resize(nul);
    }
    return trim(tail);
}

bool isConfiguredTailnum(const Config& config, const std::string& tailnum)
{
    return std::find(config.aircraft.tailnums.begin(), config.aircraft.tailnums.end(), tailnum) !=
        config.aircraft.tailnums.end();
}

bool isZiboPluginLoaded()
{
    return XPLMFindPluginBySignature("zibomod.by.Zibo") != XPLM_NO_PLUGIN_ID;
}

bool resolveRuntimeDependencies(bool logMissing)
{
    const Config& config = gController.config();
    gTailnumDataRef = XPLMFindDataRef("sim/aircraft/view/acf_tailnum");
    gTriggerDataRef = XPLMFindDataRef(sourceDataRefPath(config.source));

    const std::string tailnum = readTailnum();
    const bool tailRefReady = gTailnumDataRef != nullptr;
    const bool tailMatches = !tailnum.empty() && isConfiguredTailnum(config, tailnum);
    const bool ziboReady = !config.aircraft.requireZiboPlugin || isZiboPluginLoaded();
    const bool triggerReady = gTriggerDataRef != nullptr;

    gAutoTriggerReady = tailRefReady && tailMatches && ziboReady && triggerReady;
    if (!gAutoTriggerReady && logMissing) {
        xplaneLog(
            "auto trigger deferred: tail_ref=" + std::string(tailRefReady ? "1" : "0") +
            ", tail=" + (tailnum.empty() ? "<none>" : tailnum) +
            ", tail_match=" + std::string(tailMatches ? "1" : "0") +
            ", zibo_plugin=" + std::string(ziboReady ? "1" : "0") +
            ", trigger_dataref=" + std::string(triggerReady ? "1" : "0"));
    }
    if (gAutoTriggerReady && logMissing) {
        xplaneLog("auto trigger ready: tail=" + tailnum + ", source=" + config.source);
    }
    return gAutoTriggerReady;
}

void reloadPrefs()
{
    if (gPrefsPath.empty()) {
        gPrefsPath = prefsPath();
    }

    if (!std::filesystem::exists(gPrefsPath)) {
        fsc::stickshaker::saveDefaultConfig(gPrefsPath);
        xplaneLog("created default prefs: " + gPrefsPath.string());
    }

    Config config = fsc::stickshaker::loadConfig(gPrefsPath);
    gController.configure(std::move(config));
    gAutoTriggerReady = false;
    gNextResolveAttempt = {};
    resolveRuntimeDependencies(true);
    xplaneLog("loaded prefs: " + gPrefsPath.string());
}

bool readTrigger()
{
    if (!gTriggerDataRef) {
        return false;
    }

    const auto source = gController.config().source;
    if (sourceReadsAsFloat(source)) {
        return XPLMGetDataf(gTriggerDataRef) > 0.5f;
    }
    return XPLMGetDatai(gTriggerDataRef) != 0;
}

float flightLoopCallback(float, float, int, void*)
{
    if (!gAutoTriggerReady) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= gNextResolveAttempt) {
            const bool ready = resolveRuntimeDependencies(true);
            const int retrySec = std::max(1, gController.config().aircraft.retryIntervalSec);
            gNextResolveAttempt = now + std::chrono::seconds(retrySec);
            if (!ready && gController.config().enabled && gController.config().aircraft.deferUntilDatarefs) {
                gController.forceOff();
                return 0.5f;
            }
        }
        if (gController.config().aircraft.deferUntilDatarefs) {
            return 0.5f;
        }
    }

    gController.updateTrigger(readTrigger());
    return 0.1f;
}

int commandHandler(XPLMCommandRef command, XPLMCommandPhase phase, void*)
{
    if (phase != xplm_CommandBegin) {
        return 1;
    }

    if (command == gReloadPrefsCommand) {
        reloadPrefs();
    } else if (command == gTestOnCommand) {
        gController.forceOn();
    } else if (command == gTestOffCommand) {
        gController.forceOff();
    } else if (command == gTestPulseCommand) {
        gController.pulse();
    }

    return 1;
}

void registerCommands()
{
    gReloadPrefsCommand = XPLMCreateCommand("FSCStickShaker/reload_prefs", "Reload FSC Stick Shaker prefs");
    gTestOnCommand = XPLMCreateCommand("FSCStickShaker/test_on", "Force FSC Stick Shaker on");
    gTestOffCommand = XPLMCreateCommand("FSCStickShaker/test_off", "Force FSC Stick Shaker off");
    gTestPulseCommand = XPLMCreateCommand("FSCStickShaker/test_pulse", "Pulse FSC Stick Shaker");

    XPLMRegisterCommandHandler(gReloadPrefsCommand, commandHandler, 0, nullptr);
    XPLMRegisterCommandHandler(gTestOnCommand, commandHandler, 0, nullptr);
    XPLMRegisterCommandHandler(gTestOffCommand, commandHandler, 0, nullptr);
    XPLMRegisterCommandHandler(gTestPulseCommand, commandHandler, 0, nullptr);
}

void unregisterCommands()
{
    if (gReloadPrefsCommand) {
        XPLMUnregisterCommandHandler(gReloadPrefsCommand, commandHandler, 0, nullptr);
    }
    if (gTestOnCommand) {
        XPLMUnregisterCommandHandler(gTestOnCommand, commandHandler, 0, nullptr);
    }
    if (gTestOffCommand) {
        XPLMUnregisterCommandHandler(gTestOffCommand, commandHandler, 0, nullptr);
    }
    if (gTestPulseCommand) {
        XPLMUnregisterCommandHandler(gTestPulseCommand, commandHandler, 0, nullptr);
    }
}

} // namespace

PLUGIN_API int XPluginStart(char* outName, char* outSignature, char* outDescription)
{
    std::strncpy(outName, kPluginName, 255);
    std::strncpy(outSignature, kPluginSignature, 255);
    std::strncpy(outDescription, kPluginDescription, 255);

    fsc::stickshaker::setLogSink(xplaneLog);
    registerCommands();
    return 1;
}

PLUGIN_API void XPluginStop()
{
    XPLMUnregisterFlightLoopCallback(flightLoopCallback, nullptr);
    unregisterCommands();
    gController.shutdown();
}

PLUGIN_API int XPluginEnable()
{
    reloadPrefs();
    XPLMRegisterFlightLoopCallback(flightLoopCallback, 0.1f, nullptr);
    return 1;
}

PLUGIN_API void XPluginDisable()
{
    XPLMUnregisterFlightLoopCallback(flightLoopCallback, nullptr);
    gController.shutdown();
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int, void*)
{
}
