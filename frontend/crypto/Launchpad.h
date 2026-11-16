#pragma once

#include "BacktestingGateway.h"
#include "EventLoop.h"
#include "IMarketDataGateway.h"
#include "ITradingGateway.h"
#include "JsonStrategyConfig.h"
#include "StrategyInstance.h"

#include <memory>
#include <optional>

class Launchpad
{
public:
    Launchpad(
            const Symbol & symbol,
            const std::string & entry_strategy_name,
            const JsonStrategyConfig & entry_strategy_config,
            IMarketDataGateway & md_gateway,
            ITradingGateway & tr_gateway);

    Launchpad(
            const Symbol & symbol,
            HistoricalMDRequestData backtest_timerange,
            const std::string & entry_strategy_name,
            const JsonStrategyConfig & entry_strategy_config,
            IMarketDataGateway & source_md_gateway);

    StrategyInstance & strategy_instance();

    static std::shared_ptr<StrategyInstance> share_strategy_instance(const std::shared_ptr<Launchpad> & launchpad);

private:
    EventLoop m_event_loop;
    std::optional<BacktestingGateway> m_backtesting_gateway;

    IMarketDataGateway & m_md_gateway;
    ITradingGateway & m_tr_gateway;

    StrategyInstance m_strategy_instance;
};
