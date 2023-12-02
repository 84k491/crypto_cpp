#pragma once

#include "ByBitGateway.h"
#include "DoubleSmaStrategy.h"
#include "Signal.h"
#include "StrategyResult.h"

#include <optional>

class StrategyInstance
{
public:
    StrategyInstance(const Timerange & timerange, const DoubleSmaStrategyConfig & conf, ByBitGateway & md_gateway);

    void subscribe_for_signals(std::function<void(const Signal &)> && on_signal_cb);
    void run();

    std::map<std::string, std::vector<std::pair<std::chrono::milliseconds, double>>>
    get_strategy_internal_data_history() const;

    const StrategyResult& get_strategy_result() const;

private:
    void on_signal(const Signal & signal);
    void move_position_to(double position_size, double price);

private:
    ByBitGateway & m_md_gateway;
    DoubleSmaStrategy m_strategy;

    std::optional<Signal> m_last_signal;

    StrategyResult m_strategy_result;

    std::vector<std::function<void(const Signal &)>> m_signal_callbacks;

    double m_deposit = 0.;
    static constexpr double m_pos_currency_amount = 100.;
    double m_current_position_size = 0.;

    const Timerange m_timerange;
};
