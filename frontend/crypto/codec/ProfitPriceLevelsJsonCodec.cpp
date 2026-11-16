#include "ProfitPriceLevelsJsonCodec.h"

#include "PrimitiveJsonCodec.h"

nlohmann::json JsonCodec<ProfitPriceLevels>::encode(const ProfitPriceLevels & object)
{
    return {
            {"fee_profit_price", object.fee_profit_price},
            {"no_loss_price", object.no_loss_price},
            {"fee_loss_price", object.fee_loss_price},
    };
}

std::optional<ProfitPriceLevels> JsonCodec<ProfitPriceLevels>::decode(const nlohmann::json & json)
{
    try {
        return ProfitPriceLevels{
                .fee_profit_price = JsonCodec<double>::decode(json.at("fee_profit_price")).value(),
                .no_loss_price = JsonCodec<double>::decode(json.at("no_loss_price")).value(),
                .fee_loss_price = JsonCodec<double>::decode(json.at("fee_loss_price")).value(),
        };
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
