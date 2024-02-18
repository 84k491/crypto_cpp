#pragma once

#include "ByBitGateway.h"
#include "ByBitTradingGateway.h"
#include "ObjectPublisher.h"
#include "Position.h"
#include "Signal.h"
#include "StrategyInterface.h"
#include "StrategyResult.h"
#include "TimeseriesPublisher.h"
#include "WorkStatus.h"

#include <memory>
#include <optional>

class StrategyInstance
{
public:
    using KlineCallback = std::function<void(std::pair<std::chrono::milliseconds, OHLC>)>;
    using DepoCallback = std::function<void(std::chrono::milliseconds ts, double value)>;

    StrategyInstance(const Symbol &symbol,
                     const MarketDataRequest & md_request,
                     const std::shared_ptr<IStrategy> & strategy_ptr,
                     ByBitGateway & md_gateway,
                     ByBitTradingGateway * tr_gateway);

    TimeseriesPublisher<Signal> & signals_publisher();
    TimeseriesPublisher<std::pair<std::string, double>> & strategy_internal_data_publisher();
    TimeseriesPublisher<OHLC> & klines_publisher();
    TimeseriesPublisher<double> & depo_publisher();
    ObjectPublisher<StrategyResult> & strategy_result_publisher();
    ObjectPublisher<WorkStatus> & status_publisher();

    void run_async();
    void stop_async();
    void wait_for_finish();

private:
    void on_signal(const Signal & signal);

private:
    ByBitGateway & m_md_gateway;
    ByBitTradingGateway * m_tr_gateway;
    std::shared_ptr<IStrategy> m_strategy;

    std::optional<Signal> m_last_signal;

    ObjectPublisher<StrategyResult> m_strategy_result;

    TimeseriesPublisher<Signal> m_signal_publisher;
    TimeseriesPublisher<OHLC> m_klines_publisher;
    TimeseriesPublisher<double> m_depo_publisher;

    std::shared_ptr<TimeseriesSubsription<OHLC>> m_kline_sub;

    const Symbol m_symbol;
    static constexpr double m_pos_currency_amount = 100.;
    Position m_position;

    const MarketDataRequest m_md_request;
};
