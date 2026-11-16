#pragma once

#include "JsonCodec.h"
#include "TrailingStopLoss.h"

template <>
struct JsonCodec<StopLoss>
{
    static nlohmann::json encode(const StopLoss & object);
    static std::optional<StopLoss> decode(const nlohmann::json & json);
};
