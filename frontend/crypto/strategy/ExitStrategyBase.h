#pragma once

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

protected:
    ITradingGateway & m_tr_gateway;
};
