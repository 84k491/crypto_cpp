#pragma once

#include "EventTimeseriesPublisher.h"
#include "Signal.h"
#include "TimeseriesPublisher.h"

#include <optional>

template<class ObjectT>
class TimeseriesPublisher;

class IStrategy
{
public:
    IStrategy() = default;
    virtual ~IStrategy() = default;

    virtual std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) = 0;
    virtual EventTimeseriesPublisher<std::pair<std::string, double>>& strategy_internal_data_publisher() = 0;
    virtual bool is_valid() const = 0;
};
