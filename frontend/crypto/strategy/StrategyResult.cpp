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

double StrategyResult::apr() const
{
    const auto next_year_ts = strategy_start_ts + std::chrono::years{1};
    OLS::PriceRegressionFunction depo_trend{depo_trend_coef, depo_trend_const};

    const double next_year_depo = depo_trend(next_year_ts) + position_currency_amount;

    return 100. * (next_year_depo / position_currency_amount);
}

double StrategyResult::trades_per_month() const
{
    constexpr size_t ms_in_month = 2629800000;

    if (last_position_closed_ts == std::chrono::milliseconds{}) {
        return 0.;
    }
    if (last_position_closed_ts < strategy_start_ts) {
        return 0.;
    }

    const auto delta_ms = last_position_closed_ts.count() - strategy_start_ts.count();
    const auto delta_months = delta_ms / ms_in_month;

    return double(trades_count) / delta_months;
}
