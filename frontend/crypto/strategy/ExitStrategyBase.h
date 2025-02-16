#pragma once

#include "EventTimeseriesChannel.h"
#include "ExitStrategyInterface.h"
#include "ITradingGateway.h"

class PositionManager;

class ExitStrategyBase : public IExitStrategy
{
public:
    ~ExitStrategyBase() override = default;

    ExitStrategyBase(ITradingGateway & gateway)
        : m_tr_gateway(gateway)
    {
    }

    std::optional<std::pair<std::string, bool>>
    push_price_level(const ProfitPriceLevels &) override { return std::nullopt; }

    EventTimeseriesChannel<Tpsl> & tpsl_channel() override { return m_tpsl_channel; }
    EventTimeseriesChannel<StopLoss> & trailing_stop_channel() override { return m_trailing_stop_channel; }

protected:
    ITradingGateway & m_tr_gateway;
    EventTimeseriesChannel<Tpsl> m_tpsl_channel;
    EventTimeseriesChannel<StopLoss> m_trailing_stop_channel;
};
