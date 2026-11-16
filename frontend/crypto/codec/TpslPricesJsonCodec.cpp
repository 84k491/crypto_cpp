#include "TpslPricesJsonCodec.h"

#include "PrimitiveJsonCodec.h"

nlohmann::json JsonCodec<TpslPrices>::encode(const TpslPrices & object)
{
    return {
            {"take_profit_price", object.take_profit_price},
            {"stop_loss_price", object.stop_loss_price},
    };
}

std::optional<TpslPrices> JsonCodec<TpslPrices>::decode(const nlohmann::json & json)
{
    try {
        return TpslPrices{
                .take_profit_price = JsonCodec<double>::decode(json.at("take_profit_price")).value(),
                .stop_loss_price = JsonCodec<double>::decode(json.at("stop_loss_price")).value(),
        };
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
