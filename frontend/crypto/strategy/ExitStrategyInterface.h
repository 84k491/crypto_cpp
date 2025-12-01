#pragma once

#include "EventChannel.h"
#include "EventTimeseriesChannel.h"
#include "ConditionalOrders.h"

#include <utility>

class IExitStrategy
{
public:
    virtual ~IExitStrategy() = default;

    virtual EventTimeseriesChannel<TpslPrices> & tpsl_channel() = 0;
    virtual EventTimeseriesChannel<StopLoss> & trailing_stop_channel() = 0;

    virtual EventChannel<std::pair<std::string, bool>> & error_channel() = 0;
};
