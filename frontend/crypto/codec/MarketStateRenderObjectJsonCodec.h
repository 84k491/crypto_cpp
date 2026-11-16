#pragma once

#include "JsonCodec.h"
#include "MarketState.h"

template <>
struct JsonCodec<MarketStateRenderObject>
{
    static nlohmann::json encode(const MarketStateRenderObject & object);
    static std::optional<MarketStateRenderObject> decode(const nlohmann::json & json);
};
