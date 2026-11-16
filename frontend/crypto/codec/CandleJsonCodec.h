#pragma once

#include "Candle.h"
#include "JsonCodec.h"

template <>
struct JsonCodec<Candle>
{
    static nlohmann::json encode(const Candle & object);
    static std::optional<Candle> decode(const nlohmann::json & json);
};
