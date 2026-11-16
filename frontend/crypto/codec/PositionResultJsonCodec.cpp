#include "PositionResultJsonCodec.h"

#include "PrimitiveJsonCodec.h"

nlohmann::json JsonCodec<PositionResult>::encode(const PositionResult & object)
{
    nlohmann::json json = {
            {"guid", JsonCodec<xg::Guid>::encode(object.guid)},
            {"fees_paid", object.fees_paid},
            {"pnl_with_fee", object.pnl_with_fee},
            {"open_ts", JsonCodec<std::chrono::milliseconds>::encode(object.open_ts)},
            {"close_ts", nullptr},
    };
    if (object.close_ts.has_value()) {
        json["close_ts"] = JsonCodec<std::chrono::milliseconds>::encode(object.close_ts.value());
    }
    return json;
}

std::optional<PositionResult> JsonCodec<PositionResult>::decode(const nlohmann::json & json)
{
    try {
        PositionResult result;
        result.guid = JsonCodec<xg::Guid>::decode(json.at("guid")).value();
        result.fees_paid = JsonCodec<double>::decode(json.at("fees_paid")).value();
        result.pnl_with_fee = JsonCodec<double>::decode(json.at("pnl_with_fee")).value();
        result.open_ts = JsonCodec<std::chrono::milliseconds>::decode(json.at("open_ts")).value();
        if (!json.at("close_ts").is_null()) {
            result.close_ts = JsonCodec<std::chrono::milliseconds>::decode(json.at("close_ts")).value();
        }
        return result;
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
