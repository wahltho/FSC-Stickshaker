#include "plugin_config.h"

#include "FSCStickShaker/Config.h"
#include "FSCStickShaker/Log.h"
#include "FSCStickShaker/ShakerController.h"

#include "XPLMDataAccess.h"
#include "XPLMMenus.h"
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
#include <vector>

namespace {

using fsc::stickshaker::Config;
using fsc::stickshaker::ShakerController;

constexpr const char* kPluginVersion = PLUGIN_VERSION;
constexpr const char* kPluginName = PLUGIN_NAME;
constexpr const char* kPluginSignature = PLUGIN_SIGNATURE;
constexpr const char* kPluginDescription = PLUGIN_DESC;
constexpr const char* kPrefsFileName = PLUGIN_PREFS_FILE;
constexpr const char* kCommandPrefix = PLUGIN_COMMAND_PREFIX;
constexpr const char* kPrimaryTriggerDataRefPath = "sim/cockpit2/annunciators/stall_warning";
constexpr std::array<const char*, 4> kTriggerDataRefPaths {
    "sim/cockpit2/annunciators/stall_warning",
    "sim/cockpit2/annunciators/stall_warning_ratio",
    "laminar/B738/system/stall_1g",
    "laminar/B738/push_button/stall_test1_press",
};
constexpr int kDependencyRetryIntervalSec = 5;
constexpr bool kDeferUntilDatarefs = true;
constexpr auto kManualPulseDuration = std::chrono::seconds(2);
constexpr std::intptr_t kMenuTestOn = 1;
constexpr std::intptr_t kMenuTestOff = 2;
constexpr std::intptr_t kMenuTestPulse = 3;
constexpr std::intptr_t kMenuReloadPrefs = 4;
constexpr std::array<const char*, 2> kZiboTailnums {"ZB738", "B738"};

ShakerController gController;
std::filesystem::path gPrefsPath;
struct TriggerDataRef {
    const char* path;
    XPLMDataRef ref;
};

std::vector<TriggerDataRef> gTriggerDataRefs;
XPLMCommandRef gReloadPrefsCommand = nullptr;
XPLMCommandRef gTestOnCommand = nullptr;
XPLMCommandRef gTestOffCommand = nullptr;
XPLMCommandRef gTestPulseCommand = nullptr;
XPLMMenuID gMenu = nullptr;
int gMenuContainerIndex = -1;
XPLMDataRef gTailnumDataRef = nullptr;
bool gAutoTriggerReady = false;
bool gManualOverrideActive = false;
std::chrono::steady_clock::time_point gManualPulseEnd {};
std::chrono::steady_clock::time_point gNextResolveAttempt {};

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

bool isZiboTailnum(const std::string& tailnum)
{
    return std::find(kZiboTailnums.begin(), kZiboTailnums.end(), tailnum) != kZiboTailnums.end();
}

bool isZiboPluginLoaded()
{
    return XPLMFindPluginBySignature("zibomod.by.Zibo") != XPLM_NO_PLUGIN_ID;
}

void resolveTriggerDataRefs()
{
    gTriggerDataRefs.clear();
    for (const char* path : kTriggerDataRefPaths) {
        if (XPLMDataRef ref = XPLMFindDataRef(path)) {
            gTriggerDataRefs.push_back({path, ref});
        }
    }
}

std::string triggerSourceSummary()
{
    std::string summary;
    for (const auto& trigger : gTriggerDataRefs) {
        if (!summary.empty()) {
            summary += ",";
        }
        summary += trigger.path;
    }
    return summary.empty() ? "<none>" : summary;
}

bool resolveRuntimeDependencies(bool logMissing)
{
    gTailnumDataRef = XPLMFindDataRef("sim/aircraft/view/acf_tailnum");
    resolveTriggerDataRefs();

    const std::string tailnum = readTailnum();
    const bool tailRefReady = gTailnumDataRef != nullptr;
    const bool tailMatches = !tailnum.empty() && isZiboTailnum(tailnum);
    const bool ziboReady = isZiboPluginLoaded();
    const bool triggerReady = !gTriggerDataRefs.empty();

    gAutoTriggerReady = tailRefReady && tailMatches && ziboReady && triggerReady;
    if (!gAutoTriggerReady && logMissing) {
        xplaneLog(
            "auto trigger deferred: tail_ref=" + std::string(tailRefReady ? "1" : "0") +
            ", tail=" + (tailnum.empty() ? "<none>" : tailnum) +
            ", tail_match=" + std::string(tailMatches ? "1" : "0") +
            ", zibo_plugin=" + std::string(ziboReady ? "1" : "0") +
            ", trigger_dataref=" + std::string(triggerReady ? "1" : "0") +
            ", sources=" + triggerSourceSummary());
    }
    if (gAutoTriggerReady && logMissing) {
        xplaneLog("auto trigger ready: tail=" + tailnum + ", sources=" + triggerSourceSummary());
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
    for (const auto& trigger : gTriggerDataRefs) {
        const XPLMDataTypeID types = XPLMGetDataRefTypes(trigger.ref);
        if ((types & xplmType_Int) != 0 && XPLMGetDatai(trigger.ref) != 0) {
            return true;
        }
        if ((types & xplmType_Float) != 0 && XPLMGetDataf(trigger.ref) > 0.5f) {
            return true;
        }
    }
    return false;
}

void runReloadPrefs()
{
    reloadPrefs();
}

void runTestOn()
{
    gManualOverrideActive = true;
    gManualPulseEnd = {};
    gController.forceOn();
}

void runTestOff()
{
    gManualOverrideActive = false;
    gManualPulseEnd = {};
    gController.forceOff();
}

void runTestPulse()
{
    gManualOverrideActive = true;
    gManualPulseEnd = std::chrono::steady_clock::now() + kManualPulseDuration;
    gController.forceOn();
}

float flightLoopCallback(float, float, int, void*)
{
    const auto now = std::chrono::steady_clock::now();
    if (gManualOverrideActive) {
        if (gManualPulseEnd != std::chrono::steady_clock::time_point {} && now >= gManualPulseEnd) {
            gManualOverrideActive = false;
            gManualPulseEnd = {};
            gController.forceOff();
        }
        return 0.1f;
    }

    if (!gAutoTriggerReady) {
        if (now >= gNextResolveAttempt) {
            const bool ready = resolveRuntimeDependencies(true);
            gNextResolveAttempt = now + std::chrono::seconds(kDependencyRetryIntervalSec);
            if (!ready && gController.config().enabled && kDeferUntilDatarefs) {
                gController.updateTrigger(false);
                return 0.5f;
            }
        }
        if (kDeferUntilDatarefs) {
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
        runReloadPrefs();
    } else if (command == gTestOnCommand) {
        runTestOn();
    } else if (command == gTestOffCommand) {
        runTestOff();
    } else if (command == gTestPulseCommand) {
        runTestPulse();
    }

    return 1;
}

void menuHandler(void*, void* itemRef)
{
    const auto item = reinterpret_cast<std::intptr_t>(itemRef);
    switch (item) {
    case kMenuTestOn:
        runTestOn();
        break;
    case kMenuTestOff:
        runTestOff();
        break;
    case kMenuTestPulse:
        runTestPulse();
        break;
    case kMenuReloadPrefs:
        runReloadPrefs();
        break;
    default:
        break;
    }
}

void registerCommands()
{
    const std::string prefix = kCommandPrefix;
    gReloadPrefsCommand = XPLMCreateCommand((prefix + "/reload_prefs").c_str(), "Reload FSC Stick Shaker prefs");
    gTestOnCommand = XPLMCreateCommand((prefix + "/test_on").c_str(), "Force FSC Stick Shaker on");
    gTestOffCommand = XPLMCreateCommand((prefix + "/test_off").c_str(), "Force FSC Stick Shaker off");
    gTestPulseCommand = XPLMCreateCommand((prefix + "/test_pulse").c_str(), "Pulse FSC Stick Shaker");

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

void createMenu()
{
    if (gMenu) {
        return;
    }

    const XPLMMenuID pluginsMenu = XPLMFindPluginsMenu();
    gMenuContainerIndex = XPLMAppendMenuItem(pluginsMenu, "FSC Stick Shaker", nullptr, 1);
    gMenu = XPLMCreateMenu("FSC Stick Shaker", pluginsMenu, gMenuContainerIndex, menuHandler, nullptr);
    XPLMAppendMenuItem(gMenu, "Test On", reinterpret_cast<void*>(kMenuTestOn), 1);
    XPLMAppendMenuItem(gMenu, "Test Off", reinterpret_cast<void*>(kMenuTestOff), 1);
    XPLMAppendMenuItem(gMenu, "Test Pulse", reinterpret_cast<void*>(kMenuTestPulse), 1);
    XPLMAppendMenuSeparator(gMenu);
    XPLMAppendMenuItem(gMenu, "Reload Preferences", reinterpret_cast<void*>(kMenuReloadPrefs), 1);
}

void destroyMenu()
{
    if (!gMenu) {
        return;
    }

    XPLMDestroyMenu(gMenu);
    gMenu = nullptr;

    if (gMenuContainerIndex >= 0) {
        XPLMRemoveMenuItem(XPLMFindPluginsMenu(), gMenuContainerIndex);
        gMenuContainerIndex = -1;
    }
}

} // namespace

PLUGIN_API int XPluginStart(char* outName, char* outSignature, char* outDescription)
{
    std::strncpy(outName, kPluginName, 255);
    std::strncpy(outSignature, kPluginSignature, 255);
    const std::string description = std::string(kPluginDescription) + " (v" + kPluginVersion + ")";
    std::strncpy(outDescription, description.c_str(), 255);
    outName[255] = '\0';
    outSignature[255] = '\0';
    outDescription[255] = '\0';

    fsc::stickshaker::setLogSink(xplaneLog);
    xplaneLog(std::string("Plugin version ") + kPluginVersion);
    registerCommands();
    createMenu();
    return 1;
}

PLUGIN_API void XPluginStop()
{
    XPLMUnregisterFlightLoopCallback(flightLoopCallback, nullptr);
    destroyMenu();
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
