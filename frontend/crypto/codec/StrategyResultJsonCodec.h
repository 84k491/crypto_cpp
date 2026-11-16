#pragma once

#include "JsonCodec.h"
#include "StrategyResult.h"

template <>
struct JsonCodec<StrategyResult>
{
    static nlohmann::json encode(const StrategyResult & object);
    static std::optional<StrategyResult> decode(const nlohmann::json & json);
};
