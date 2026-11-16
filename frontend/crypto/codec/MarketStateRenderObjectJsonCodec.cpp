#include "MarketStateRenderObjectJsonCodec.h"

#include "PrimitiveJsonCodec.h"

namespace {

std::optional<MarketState> decode_market_state(const std::string & text)
{
    for (const auto state : {
                 MarketState::None,
                 MarketState::NoTrend,
                 MarketState::UpTrend,
                 MarketState::DownTrend,
                 MarketState::HighVolatility,
         }) {
        if (enumToString(state) == text) {
            return state;
        }
    }
    return std::nullopt;
}

} // namespace

nlohmann::json JsonCodec<MarketStateRenderObject>::encode(const MarketStateRenderObject & object)
{
    return {
            {"upper_limit", object.upper_limit},
            {"lower_limit", object.lower_limit},
            {"state", enumToString(object.state)},
    };
}

std::optional<MarketStateRenderObject> JsonCodec<MarketStateRenderObject>::decode(const nlohmann::json & json)
{
    try {
        return MarketStateRenderObject{
                .upper_limit = JsonCodec<double>::decode(json.at("upper_limit")).value(),
                .lower_limit = JsonCodec<double>::decode(json.at("lower_limit")).value(),
                .state = decode_market_state(json.at("state").get<std::string>()).value(),
        };
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
