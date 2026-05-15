#pragma once

#include "FSCStickShaker/Config.h"

#include <memory>
#include <string>

namespace fsc::stickshaker {

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool open(const Config& config) = 0;
    virtual void close() = 0;
    virtual bool send(bool active) = 0;
    virtual std::string name() const = 0;
};

class LoggingTransport final : public ITransport {
public:
    bool open(const Config& config) override;
    void close() override;
    bool send(bool active) override;
    std::string name() const override;

private:
    TransportKind selectedTransport_ = TransportKind::LogOnly;
};

std::unique_ptr<ITransport> makeTransport(const Config& config);

} // namespace fsc::stickshaker
