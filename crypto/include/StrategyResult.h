#pragma once

#include <chrono>
#include <cstddef>

class StrategyResult
{
public:
    double position_currency_amount = 0.;
    double final_profit = 0.;
    size_t trades_count = 0;
    double fees_paid = 0.;
    double profit_per_trade = 0.;
    double best_profit_trade = 0.;
    double worst_loss_trade = 0.;
    double max_depo = 0.;
    double min_depo = 0.;
    std::chrono::seconds longest_profit_trade_time = {};
    std::chrono::seconds longest_loss_trade_time = {};
};
