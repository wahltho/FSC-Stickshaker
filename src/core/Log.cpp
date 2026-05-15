#include "FSCStickShaker/Log.h"

#include <iostream>
#include <utility>

namespace fsc::stickshaker {
namespace {

LogSink& sink()
{
    static LogSink current = [](const std::string& message) {
        std::cerr << "[FSCStickShaker] " << message << '\n';
    };
    return current;
}

} // namespace

void setLogSink(LogSink sink)
{
    if (sink) {
        fsc::stickshaker::sink() = std::move(sink);
    }
}

void logInfo(const std::string& message)
{
    sink()(message);
}

void logDebug(bool enabled, const std::string& message)
{
    if (enabled) {
        logInfo(message);
    }
}

} // namespace fsc::stickshaker
