#pragma once

#include "JsonCodec.h"
#include "Trade.h"

template <>
struct JsonCodec<Trade>
{
    static nlohmann::json encode(const Trade & object);
    static std::optional<Trade> decode(const nlohmann::json & json);
};
