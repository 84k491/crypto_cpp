#include "StrategyResult.h"

std::ostream & operator<<(std::ostream & out, const StrategyResult & result)
{
    out << "position_currency_amount: " << result.position_currency_amount << std::endl;
    out << "final_profit: " << result.final_profit << std::endl;
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
    return out;
}
