#pragma once

#include "ByBitGateway.h"
#include "DoubleSmaStrategy.h"
#include "Position.h"
#include "Signal.h"
#include "StrategyResult.h"

#include <optional>

class StrategyInstance
{
public:
    using KlineCallback = std::function<void(std::pair<std::chrono::milliseconds, OHLC>)>;
    using DepoCallback = std::function<void(std::chrono::milliseconds ts, double value)>;

    StrategyInstance(const Timerange & timerange, const DoubleSmaStrategyConfig & conf, ByBitGateway & md_gateway);

    void subscribe_for_signals(std::function<void(const Signal &)> && on_signal_cb);
    void subscribe_for_strategy_internal(std::function<void(std::string name,
                                                            std::chrono::milliseconds ts,
                                                            double data)> && cb);
    void subscribe_for_klines(KlineCallback && on_kline_received_cb);
    void subscribe_for_depo(DepoCallback && on_depo_cb);
    void run();

    std::map<std::string, std::vector<std::pair<std::chrono::milliseconds, double>>>
    get_strategy_internal_data_history() const;

    const StrategyResult & get_strategy_result() const;

private:
    void on_signal(const Signal & signal);

private:
    ByBitGateway & m_md_gateway;
    DoubleSmaStrategy m_strategy;

    std::optional<Signal> m_last_signal;

    StrategyResult m_strategy_result;

    std::vector<std::function<void(const Signal &)>> m_signal_callbacks;
    std::vector<std::function<void(std::string name,
                                   std::chrono::milliseconds ts,
                                   double data)>>
            m_strategy_internal_callbacks;

    double m_deposit = 0.; // TODO use result profit
    static constexpr double m_pos_currency_amount = 100.;
    Position m_position;

    const Timerange m_timerange;

    std::vector<KlineCallback> m_kline_callbacks;
    std::vector<DepoCallback> m_depo_callbacks;

    double m_best_profit = std::numeric_limits<double>::lowest();
    double m_worst_loss = 0.;
};
