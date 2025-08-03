#pragma once

#include "EventTimeseriesChannel.h"

#include <optional>

class IStrategy
{
public:
    virtual ~IStrategy() = default;

    virtual EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() = 0;
    virtual bool is_valid() const = 0;
    virtual std::optional<std::chrono::milliseconds> timeframe() const = 0;
    virtual bool export_price_levels() const { return true; }
};
