#pragma once

#include "ByBitGateway.h"
#include "Position.h"
#include "Signal.h"
#include "StrategyInterface.h"
#include "StrategyResult.h"
#include "TimeseriesPublisher.h"

#include <optional>

class StrategyInstance
{
public:
    using KlineCallback = std::function<void(std::pair<std::chrono::milliseconds, OHLC>)>;
    using DepoCallback = std::function<void(std::chrono::milliseconds ts, double value)>;

    StrategyInstance(const Timerange & timerange,
                     const std::shared_ptr<IStrategy> & strategy_ptr,
                     ByBitGateway & md_gateway);

    TimeseriesPublisher<Signal>& signals_publisher();
    TimeseriesPublisher<std::pair<std::string, double>>& strategy_internal_data_publisher();
    TimeseriesPublisher<OHLC>& klines_publisher();
    TimeseriesPublisher<double>& depo_publisher();

    bool run(const Symbol & symbol);

    std::map<std::string, std::vector<std::pair<std::chrono::milliseconds, double>>>
    get_strategy_internal_data_history() const;

    const StrategyResult & get_strategy_result() const;

private:
    void on_signal(const Signal & signal);

private:
    ByBitGateway & m_md_gateway;
    std::shared_ptr<IStrategy> m_strategy;

    std::optional<Signal> m_last_signal;

    StrategyResult m_strategy_result;

    TimeseriesPublisher<Signal> m_signal_publisher;
    TimeseriesPublisher<OHLC> m_klines_publisher;
    TimeseriesPublisher<double> m_depo_publisher;

    static constexpr double m_pos_currency_amount = 100.;
    Position m_position;

    const Timerange m_timerange;

    double m_deposit = 0.; // TODO use result profit
    double m_best_profit = std::numeric_limits<double>::lowest();
    double m_worst_loss = 0.;
};
