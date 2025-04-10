#include "StrategyResult.h"
#include "OrdinaryLeastSquares.h"

std::ostream & operator<<(std::ostream & out, const StrategyResult & result)
{
    const auto j = result.to_json();
    out << j;
    return out;
}

nlohmann::json StrategyResult::to_json() const
{
    return {
            {"position_currency_amount", position_currency_amount},
            {"final_profit", final_profit},
            {"depo_standard_deviation", depo_standard_deviation},
            {"trades_count", trades_count},
            {"last_trade_date", last_trade_date},
            {"profit_poitions_count", profit_positions_cnt},
            {"loss_poitions_count", loss_positions_cnt},
            {"fees_paid", fees_paid},
            {"profit_per_trade", profit_per_trade()},
            {"best_profit_trade", best_profit_trade.value_or(0.)},
            {"worst_loss_trade", worst_loss_trade.value_or(0.)},
            {"max_depo", max_depo},
            {"min_depo", min_depo},
            {"avg_profit_pos_time", avg_profit_pos_time},
            {"avg_loss_pos_time", avg_loss_pos_time},
            {"longest_profit_trade_time", longest_profit_trade_time.count()},
            {"longest_loss_trade_time", longest_loss_trade_time.count()},
            {"depo_trend_coef", depo_trend_coef},
            {"depo_trend_const", depo_trend_const},
            {"first_position_closed_ts", first_position_closed_ts.count()},
            {"last_position_closed_ts", last_position_closed_ts.count()},
    };
}

void StrategyResult::set_trend_info(const std::vector<std::pair<std::chrono::milliseconds, double>> & prices)
{
    if (prices.empty()) {
        return;
    }

    const auto [reg, dev] = OLS::solve_prices(prices);

    depo_standard_deviation = dev;
    depo_trend_coef = reg.k;
    depo_trend_const = reg.c;
    last_position_closed_ts = prices.back().first;
    first_position_closed_ts = prices.front().first;
}
