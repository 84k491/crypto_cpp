#pragma once

#include "ConditionalOrders.h"
#include "JsonCodec.h"

template <>
struct JsonCodec<TpslPrices>
{
    static nlohmann::json encode(const TpslPrices & object);
    static std::optional<TpslPrices> decode(const nlohmann::json & json);
};
