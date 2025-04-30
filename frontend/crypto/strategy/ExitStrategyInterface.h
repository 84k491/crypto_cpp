#pragma once

#include "EventTimeseriesChannel.h"
#include "EventObjectChannel.h"
#include "Position.h"
#include "Trade.h"

#include <chrono>
#include <utility>

class IExitStrategy
{
public:
    virtual ~IExitStrategy() = default;

    virtual EventTimeseriesChannel<Tpsl> & tpsl_channel() = 0;
    virtual EventTimeseriesChannel<StopLoss> & trailing_stop_channel() = 0;

    virtual EventObjectChannel<std::pair<std::string, bool>> & error_channel() = 0;
};
