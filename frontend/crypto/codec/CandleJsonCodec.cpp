#include "CandleJsonCodec.h"

#include "PrimitiveJsonCodec.h"

nlohmann::json JsonCodec<Candle>::encode(const Candle & object)
{
    return object.to_json();
}

std::optional<Candle> JsonCodec<Candle>::decode(const nlohmann::json & json)
{
    try {
        const auto start_ts = JsonCodec<std::chrono::milliseconds>::decode(json.at("ts")).value();
        const auto close_ts = JsonCodec<std::chrono::milliseconds>::decode(json.at("close_ts")).value();

        return Candle{
                close_ts - start_ts,
                start_ts,
                JsonCodec<double>::decode(json.at("open")).value(),
                JsonCodec<double>::decode(json.at("high")).value(),
                JsonCodec<double>::decode(json.at("low")).value(),
                JsonCodec<double>::decode(json.at("close")).value(),
                JsonCodec<double>::decode(json.at("buy_volume")).value(),
                JsonCodec<double>::decode(json.at("sell_volume")).value(),
                json.at("trade_count").get<size_t>(),
        };
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
