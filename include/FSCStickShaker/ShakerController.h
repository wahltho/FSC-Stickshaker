#pragma once

#include "FSCStickShaker/Config.h"
#include "FSCStickShaker/Transport.h"

#include <chrono>
#include <memory>
#include <optional>

namespace fsc::stickshaker {

class ShakerController {
public:
    void configure(Config config);
    void shutdown();

    void updateTrigger(bool active);
    void forceOn();
    void forceOff();
    void pulse();

    const Config& config() const;

private:
    bool sendState(bool active, bool force);

    Config config_;
    std::unique_ptr<ITransport> transport_;
    std::optional<bool> lastSentState_;
    std::chrono::steady_clock::time_point nextTransportOpenAttempt_ {};
};

} // namespace fsc::stickshaker
