#pragma once

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
        // TODO this is copy from downloader, move it to util
        const time_t t = std::chrono::duration_cast<std::chrono::seconds>(ts).count();
        std::array<char, 30> s{};
        std::strftime(s.data(), s.size(), "%Y-%m-%d", std::gmtime(&t));
        last_trade_date = s.data();
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
    std::chrono::seconds longest_profit_trade_time = {};
    std::chrono::seconds longest_loss_trade_time = {};
};
