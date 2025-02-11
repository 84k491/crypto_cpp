#pragma once

#include "Candle.h"
#include "EventTimeseriesChannel.h"
#include "Signal.h"

#include <optional>

class IStrategy
{
public:
    IStrategy() = default;
    virtual ~IStrategy() = default;

    virtual std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) = 0;
    virtual std::optional<Signal> push_candle(const Candle & c) = 0;
    virtual EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() = 0;
    virtual bool is_valid() const = 0;
    virtual std::optional<std::chrono::milliseconds> timeframe() const = 0;
};
