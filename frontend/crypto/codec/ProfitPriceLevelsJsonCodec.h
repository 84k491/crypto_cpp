#pragma once

#include "JsonCodec.h"
#include "Position.h"

template <>
struct JsonCodec<ProfitPriceLevels>
{
    static nlohmann::json encode(const ProfitPriceLevels & object);
    static std::optional<ProfitPriceLevels> decode(const nlohmann::json & json);
};
