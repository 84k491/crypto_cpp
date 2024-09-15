#pragma once

#include "EventTimeseriesPublisher.h"
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

    EventTimeseriesPublisher<Tpsl> & tpsl_publisher() override { return m_tpsl_publisher; }
    EventTimeseriesPublisher<StopLoss> & trailing_stop_publisher() override { return m_trailing_stop_publisher; }

protected:
    ITradingGateway & m_tr_gateway;
    EventTimeseriesPublisher<Tpsl> m_tpsl_publisher;
    EventTimeseriesPublisher<StopLoss> m_trailing_stop_publisher;
};
