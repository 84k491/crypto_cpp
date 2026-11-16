#include "StrategyResultJsonCodec.h"

#include "PrimitiveJsonCodec.h"

nlohmann::json JsonCodec<StrategyResult>::encode(const StrategyResult & object)
{
    return object.to_json();
}

std::optional<StrategyResult> JsonCodec<StrategyResult>::decode(const nlohmann::json & json)
{
    try {
        const auto decode_number = [&](const char * key) {
            return JsonCodec<double>::decode(json.at(key)).value();
        };
        const auto decode_milliseconds = [&](const char * key) {
            return JsonCodec<std::chrono::milliseconds>::decode(json.at(key)).value();
        };

        StrategyResult result;
        result.position_currency_amount = decode_number("position_currency_amount");
        result.strategy_start_ts = decode_milliseconds("strategy_start_ts");
        result.final_profit = decode_number("final_profit");
        result.depo_standard_deviation = decode_number("depo_standard_deviation");
        result.trades_count = json.at("trades_count").get<size_t>();
        result.last_trade_date = json.at("last_trade_date").get<std::string>();
        result.profit_positions_cnt = json.at("profit_positions_count").get<size_t>();
        result.loss_positions_cnt = json.at("loss_positions_count").get<size_t>();
        result.fees_paid = decode_number("fees_paid");
        result.best_profit_trade = decode_number("best_profit_trade");
        result.worst_loss_trade = decode_number("worst_loss_trade");
        result.max_depo = decode_number("max_depo");
        result.min_depo = decode_number("min_depo");
        result.avg_profit_pos_time = decode_number("avg_profit_pos_time");
        result.avg_loss_pos_time = decode_number("avg_loss_pos_time");
        result.longest_profit_trade_time = std::chrono::seconds{json.at("longest_profit_trade_time").get<int64_t>()};
        result.longest_loss_trade_time = std::chrono::seconds{json.at("longest_loss_trade_time").get<int64_t>()};
        result.depo_trend_coef = decode_number("depo_trend_coef");
        result.depo_trend_const = decode_number("depo_trend_const");
        result.first_position_closed_ts = decode_milliseconds("first_position_closed_ts");
        result.last_position_closed_ts = decode_milliseconds("last_position_closed_ts");
        return result;
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
