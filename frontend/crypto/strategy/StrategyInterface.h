#pragma once

#include "EventChannel.h"
#include "EventTimeseriesChannel.h"

#include <optional>

struct StrategyInternalData
{
    std::string_view chart_name;
    std::string_view series_name;
    double value;
};

class IStrategy
{
public:
    virtual ~IStrategy() = default;

    virtual EventTimeseriesChannel<StrategyInternalData> & strategy_internal_data_channel() = 0;
    virtual EventChannel<std::pair<std::string, bool>> & error_channel() = 0;

    virtual bool is_valid() const = 0;
    virtual std::optional<std::chrono::milliseconds> timeframe() const = 0;
    virtual bool export_price_levels() const { return true; }
};
