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
    double fees_paid = 0.;
    double profit_per_trade() const { return final_profit / static_cast<double>(trades_count); }
    std::optional<double> best_profit_trade = 0.;
    std::optional<double> worst_loss_trade = 0.;
    double max_depo = 0.;
    double min_depo = 0.;
    std::chrono::seconds longest_profit_trade_time = {};
    std::chrono::seconds longest_loss_trade_time = {};
};
std::ostream & operator<<(std::ostream & out, const StrategyResult & result);
