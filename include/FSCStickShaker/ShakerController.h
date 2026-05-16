#pragma once

#include "FSCStickShaker/Config.h"
#include "FSCStickShaker/Protocol.h"
#include "FSCStickShaker/Transport.h"

#include <chrono>
#include <memory>
#include <optional>

namespace fsc::stickshaker {

class ShakerController {
public:
    void configure(Config config);
    void shutdown();

    void updateTrigger(ShakerState state);
    void forceOn();
    void forceOff();
    void pulse();

    const Config& config() const;

private:
    bool sendState(ShakerState state, bool force);

    Config config_;
    std::unique_ptr<ITransport> transport_;
    std::optional<ShakerState> lastSentState_;
    std::chrono::steady_clock::time_point nextTransportOpenAttempt_ {};
};

} // namespace fsc::stickshaker
