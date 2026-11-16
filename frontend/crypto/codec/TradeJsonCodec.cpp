#include "TradeJsonCodec.h"

#include "PrimitiveJsonCodec.h"
#include "SideJsonCodec.h"

nlohmann::json JsonCodec<Trade>::encode(const Trade & object)
{
    return {
            {"ts", JsonCodec<std::chrono::milliseconds>::encode(object.ts())},
            {"symbol_name", object.symbol_name()},
            {"order_guid", JsonCodec<xg::Guid>::encode(object.order_guid())},
            {"price", object.price()},
            {"volume", object.unsigned_volume().value()},
            {"side", JsonCodec<Side>::encode(object.side())},
            {"fee", object.fee()},
    };
}

std::optional<Trade> JsonCodec<Trade>::decode(const nlohmann::json & json)
{
    try {
        const auto volume_opt = UnsignedVolume::from(JsonCodec<double>::decode(json.at("volume")).value());
        if (!volume_opt.has_value()) {
            return std::nullopt;
        }

        return Trade{
                JsonCodec<std::chrono::milliseconds>::decode(json.at("ts")).value(),
                json.at("symbol_name").get<std::string>(),
                JsonCodec<xg::Guid>::decode(json.at("order_guid")).value(),
                JsonCodec<double>::decode(json.at("price")).value(),
                volume_opt.value(),
                JsonCodec<Side>::decode(json.at("side")).value(),
                JsonCodec<double>::decode(json.at("fee")).value(),
        };
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
