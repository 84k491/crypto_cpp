#pragma once

#include "JsonCodec.h"
#include "PositionManager.h"

template <>
struct JsonCodec<PositionResult>
{
    static nlohmann::json encode(const PositionResult & object);
    static std::optional<PositionResult> decode(const nlohmann::json & json);
};
