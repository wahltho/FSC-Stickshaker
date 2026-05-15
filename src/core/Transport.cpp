#include "FSCStickShaker/Transport.h"

#include "FSCStickShaker/Log.h"
#include "FSCStickShaker/Protocol.h"

#include <memory>

namespace fsc::stickshaker {

bool LoggingTransport::open(const Config& config)
{
    selectedTransport_ = config.transport;
    logInfo("transport open: " + name());
    return true;
}

void LoggingTransport::close()
{
    logInfo("transport close: " + name());
}

bool LoggingTransport::send(bool active)
{
    if (selectedTransport_ == TransportKind::Tcp) {
        const auto frames = tcpFrames(active);
        logInfo(std::string("TCP send stub: ") + frames[0] + " then " + frames[1]);
    } else {
        logInfo("serial send stub: " + bytesToHex(serialFrame(active)));
    }
    return true;
}

std::string LoggingTransport::name() const
{
    return toString(selectedTransport_);
}

std::unique_ptr<ITransport> makeTransport(const Config&)
{
    return std::make_unique<LoggingTransport>();
}

} // namespace fsc::stickshaker
