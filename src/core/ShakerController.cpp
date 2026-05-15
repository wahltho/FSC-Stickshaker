#include "FSCStickShaker/ShakerController.h"

#include "FSCStickShaker/Log.h"

#include <chrono>
#include <utility>

namespace fsc::stickshaker {
namespace {

constexpr auto kTransportRetryInterval = std::chrono::seconds(5);

} // namespace

void ShakerController::configure(Config config)
{
    shutdown();

    config_ = std::move(config);
    transport_ = makeTransport(config_);
    lastSentState_.reset();
    nextTransportOpenAttempt_ = {};

    if (!config_.enabled) {
        logInfo("controller configured but disabled");
        return;
    }

    if (!transport_->open(config_)) {
        logInfo("transport failed to open: " + transport_->name());
        transport_.reset();
        nextTransportOpenAttempt_ = std::chrono::steady_clock::now() + kTransportRetryInterval;
    }
}

void ShakerController::shutdown()
{
    if (transport_) {
        if (lastSentState_.value_or(false)) {
            transport_->send(false);
        }
        transport_->close();
        transport_.reset();
    }
    lastSentState_.reset();
}

void ShakerController::updateTrigger(bool active)
{
    if (!config_.enabled) {
        return;
    }
    sendState(active, false);
}

void ShakerController::forceOn()
{
    sendState(true, true);
}

void ShakerController::forceOff()
{
    sendState(false, true);
}

void ShakerController::pulse()
{
    forceOn();
    forceOff();
}

const Config& ShakerController::config() const
{
    return config_;
}

bool ShakerController::sendState(bool active, bool force)
{
    if (!transport_) {
        const auto now = std::chrono::steady_clock::now();
        if (!force && now < nextTransportOpenAttempt_) {
            return false;
        }
        transport_ = makeTransport(config_);
        if (!transport_->open(config_)) {
            logInfo("transport failed to open: " + transport_->name());
            transport_.reset();
            nextTransportOpenAttempt_ = now + kTransportRetryInterval;
            return false;
        }
        nextTransportOpenAttempt_ = {};
    }

    if (!force && lastSentState_.has_value() && *lastSentState_ == active) {
        logDebug(config_.debug, active ? "suppress duplicate ON" : "suppress duplicate OFF");
        return true;
    }

    const bool sent = transport_->send(active);
    if (sent) {
        lastSentState_ = active;
    } else {
        transport_->close();
        transport_.reset();
        nextTransportOpenAttempt_ = std::chrono::steady_clock::now() + kTransportRetryInterval;
    }
    return sent;
}

} // namespace fsc::stickshaker
