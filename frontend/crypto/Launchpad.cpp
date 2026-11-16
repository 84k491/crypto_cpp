#include "Launchpad.h"

Launchpad::Launchpad(
        const Symbol & symbol,
        const std::string & entry_strategy_name,
        const JsonStrategyConfig & entry_strategy_config,
        IMarketDataGateway & md_gateway,
        ITradingGateway & tr_gateway)
    : m_md_gateway(md_gateway)
    , m_tr_gateway(tr_gateway)
    , m_strategy_instance(
              m_event_loop,
              symbol,
              entry_strategy_name,
              entry_strategy_config,
              m_md_gateway,
              m_tr_gateway)
{
}

Launchpad::Launchpad(
        const Symbol & symbol,
        HistoricalMDRequestData backtest_timerange,
        const std::string & entry_strategy_name,
        const JsonStrategyConfig & entry_strategy_config,
        IMarketDataGateway & source_md_gateway)
    : m_backtesting_gateway(std::in_place, m_event_loop, source_md_gateway, backtest_timerange)
    , m_md_gateway(*m_backtesting_gateway)
    , m_tr_gateway(*m_backtesting_gateway)
    , m_strategy_instance(
              m_event_loop,
              symbol,
              entry_strategy_name,
              entry_strategy_config,
              m_md_gateway,
              m_tr_gateway)
{
}

StrategyInstance & Launchpad::strategy_instance()
{
    return m_strategy_instance;
}

BacktestingGateway * Launchpad::backtesting_gateway()
{
    if (!m_backtesting_gateway.has_value()) {
        return nullptr;
    }

    return &m_backtesting_gateway.value();
}

std::shared_ptr<StrategyInstance> Launchpad::share_strategy_instance(const std::shared_ptr<Launchpad> & launchpad)
{
    return {launchpad, &launchpad->m_strategy_instance};
}
