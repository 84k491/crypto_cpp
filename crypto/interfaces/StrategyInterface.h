#pragma once

#include "Signal.h"

#include <functional>
#include <optional>

class IStrategy
{
public:
    IStrategy() = default;
    virtual ~IStrategy() = default;

    virtual std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) = 0;

    virtual void subscribe_for_strategy_internal(std::function<void(std::string name,
                                                                    std::chrono::milliseconds ts,
                                                                    double data)> && cb) = 0;

    virtual bool is_valid() const = 0;
};
