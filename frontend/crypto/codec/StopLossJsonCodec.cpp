#include "StopLossJsonCodec.h"

#include "PrimitiveJsonCodec.h"
#include "SideJsonCodec.h"

nlohmann::json JsonCodec<StopLoss>::encode(const StopLoss & object)
{
    return {
            {"symbol_name", object.symbol_name()},
            {"stop_price", object.stop_price()},
            {"side", JsonCodec<Side>::encode(object.side())},
    };
}

std::optional<StopLoss> JsonCodec<StopLoss>::decode(const nlohmann::json & json)
{
    try {
        return StopLoss{
                json.at("symbol_name").get<std::string>(),
                JsonCodec<double>::decode(json.at("stop_price")).value(),
                JsonCodec<Side>::decode(json.at("side")).value(),
        };
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
