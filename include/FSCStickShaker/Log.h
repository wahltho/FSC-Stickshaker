#pragma once

#include <functional>
#include <string>

namespace fsc::stickshaker {

using LogSink = std::function<void(const std::string&)>;

void setLogSink(LogSink sink);
void logInfo(const std::string& message);
void logDebug(bool enabled, const std::string& message);

} // namespace fsc::stickshaker
