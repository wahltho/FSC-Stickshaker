#pragma once

#include "FSCStickShaker/Config.h"
#include "FSCStickShaker/Transport.h"

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
};

} // namespace fsc::stickshaker
