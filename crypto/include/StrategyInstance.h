#pragma once

#include "ByBitGateway.h"
#include "ObjectPublisher.h"
#include "Position.h"
#include "Signal.h"
#include "StrategyInterface.h"
#include "StrategyResult.h"
#include "TimeseriesPublisher.h"
#include "WorkStatus.h"
#include "WorkerThread.h"

#include <memory>
#include <optional>

class StrategyInstance
{
public:
    using KlineCallback = std::function<void(std::pair<std::chrono::milliseconds, OHLC>)>;
    using DepoCallback = std::function<void(std::chrono::milliseconds ts, double value)>;

    StrategyInstance(const MarketDataRequest & md_request,
                     const std::shared_ptr<IStrategy> & strategy_ptr,
                     ByBitGateway & md_gateway);

    TimeseriesPublisher<Signal> & signals_publisher();
    TimeseriesPublisher<std::pair<std::string, double>> & strategy_internal_data_publisher();
    TimeseriesPublisher<OHLC> & klines_publisher();
    TimeseriesPublisher<double> & depo_publisher();
    ObjectPublisher<StrategyResult> & strategy_result_publisher();
    ObjectPublisher<WorkStatus> & status_publisher();

    void run_async(const Symbol & symbol);
    void stop_async();
    void wait_for_finish();

private:
    bool do_run(const Symbol & symbol);
    void on_signal(const Signal & signal);

private:
    ByBitGateway & m_md_gateway;
    std::shared_ptr<IStrategy> m_strategy;

    std::unique_ptr<WorkerThread> m_worker_thread;

    std::optional<Signal> m_last_signal;

    ObjectPublisher<StrategyResult> m_strategy_result;

    TimeseriesPublisher<Signal> m_signal_publisher;
    TimeseriesPublisher<OHLC> m_klines_publisher;
    TimeseriesPublisher<double> m_depo_publisher;

    static constexpr double m_pos_currency_amount = 100.;
    Position m_position;

    const MarketDataRequest m_md_request;

    double m_deposit = 0.; // TODO use result profit
    double m_best_profit = std::numeric_limits<double>::lowest();
    double m_worst_loss = 0.;
};
