#pragma once

#include "EventChannel.h"
#include "EventTimeseriesChannel.h"
#include "ExitStrategyInterface.h"

class PositionManager;

class ExitStrategyBase : public IExitStrategy
{
public:
    ~ExitStrategyBase() override = default;

    ExitStrategyBase()
    = default;

    EventTimeseriesChannel<TpslPrices> & tpsl_channel() override { return m_tpsl_channel; }
    EventTimeseriesChannel<StopLoss> & trailing_stop_channel() override { return m_trailing_stop_channel; }
    EventChannel<std::pair<std::string, bool>> & error_channel() override { return m_error_channel; }

protected:
    EventTimeseriesChannel<TpslPrices> m_tpsl_channel;
    EventTimeseriesChannel<StopLoss> m_trailing_stop_channel;
    EventChannel<std::pair<std::string, bool>> m_error_channel;
};
