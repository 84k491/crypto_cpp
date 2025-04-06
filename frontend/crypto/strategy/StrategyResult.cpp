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
            {"first_depo_trend_value", first_depo_trend_value},
            {"first_depo_trend_ts", first_depo_trend_ts.count()},
            {"last_depo_trend_value", last_depo_trend_value},
            {"last_depo_trend_ts", last_depo_trend_ts.count()},
    };
}

void StrategyResult::set_trend_info(const std::vector<std::pair<std::chrono::milliseconds, double>> & prices)
{
    if (prices.empty()) {
        return;
    }

    const auto [reg, dev] = OLS::solve_prices(prices);

    depo_standard_deviation = dev;
    last_depo_trend_value = reg(prices.back().first);
    last_depo_trend_ts = prices.back().first;
    first_depo_trend_value = reg(prices.front().first);
    first_depo_trend_ts = prices.front().first;
}
