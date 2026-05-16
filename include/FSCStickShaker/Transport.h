#pragma once

#include "FSCStickShaker/Config.h"
#include "FSCStickShaker/Protocol.h"

#include <memory>
#include <string>

namespace fsc::stickshaker {

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool open(const Config& config) = 0;
    virtual void close() = 0;
    virtual bool send(ShakerState state) = 0;
    virtual std::string name() const = 0;
};

std::unique_ptr<ITransport> makeTransport(const Config& config);

} // namespace fsc::stickshaker
