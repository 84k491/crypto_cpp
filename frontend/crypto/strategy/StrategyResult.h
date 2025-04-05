#pragma once

#include "DateTimeConverter.h"

#include <chrono>
#include <cstddef>
#include <optional>

class StrategyResult
{
    friend std::ostream & operator<<(std::ostream & out, const StrategyResult & result);

public:
    double position_currency_amount = 0.;
    double final_profit = 0.;
    size_t trades_count = 0;
    std::string last_trade_date;
    size_t profit_positions_cnt = 0;
    size_t loss_positions_cnt = 0;

    void set_last_trade_date(std::chrono::milliseconds ts)
    {
        last_trade_date = DateTimeConverter::date_time(ts);
    }

    double win_rate() const
    {
        return static_cast<double>(profit_positions_cnt) / static_cast<double>(profit_positions_cnt + loss_positions_cnt);
    }

    double fees_paid = 0.;

    double profit_per_trade() const { return final_profit / static_cast<double>(trades_count); }

    std::optional<double> best_profit_trade = 0.;
    std::optional<double> worst_loss_trade = 0.;
    double max_depo = 0.;
    double min_depo = 0.;

    std::chrono::seconds total_time_in_profit_pos = {};
    std::chrono::seconds total_time_in_loss_pos = {};

    double avg_profit_pos_time = {};
    double avg_loss_pos_time = {};

    std::chrono::seconds longest_profit_trade_time = {};
    std::chrono::seconds longest_loss_trade_time = {};

    void set_trend_info(const std::vector<std::pair<std::chrono::milliseconds, double>> & prices);
    double last_depo_trend_value = 0.;
    std::chrono::milliseconds last_depo_trend_ts;
    double first_depo_trend_value = 0.;
    std::chrono::milliseconds first_depo_trend_ts;
    double depo_standard_deviation = 0.;
};
