#include "StrategyResult.h"
#include "OrdinaryLeastSquares.h"

std::ostream & operator<<(std::ostream & out, const StrategyResult & result)
{
    out << "position_currency_amount: " << result.position_currency_amount << std::endl;
    out << "final_profit: " << result.final_profit << std::endl;
    out << "depo_standard_deviation: " << result.depo_standard_deviation << std::endl;
    out << "trades_count: " << result.trades_count << std::endl;
    out << "last_trade_date: " << result.last_trade_date << std::endl;
    out << "profit_poitions_count: " << result.profit_positions_cnt << std::endl;
    out << "loss_poitions_count: " << result.loss_positions_cnt << std::endl;
    out << "fees_paid: " << result.fees_paid << std::endl;
    out << "profit_per_trade: " << result.profit_per_trade() << std::endl;
    out << "best_profit_trade: " << result.best_profit_trade.value_or(0.) << std::endl;
    out << "worst_loss_trade: " << result.worst_loss_trade.value_or(0.) << std::endl;
    out << "max_depo: " << result.max_depo << std::endl;
    out << "min_depo: " << result.min_depo << std::endl;
    out << "avg_profit_pos_time: " << result.avg_profit_pos_time << std::endl;
    out << "avg_loss_pos_time: " << result.avg_loss_pos_time << std::endl;
    out << "longest_profit_trade_time: " << result.longest_profit_trade_time.count() << std::endl;
    out << "longest_loss_trade_time: " << result.longest_loss_trade_time.count() << std::endl;
    out << "first_depo_trend_value: " << result.first_depo_trend_value << std::endl;
    out << "first_depo_trend_ts: " << result.first_depo_trend_ts.count() << std::endl;
    out << "last_depo_trend_value: " << result.last_depo_trend_value << std::endl;
    out << "last_depo_trend_ts: " << result.last_depo_trend_ts.count() << std::endl;
    return out;
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
