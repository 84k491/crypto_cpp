#pragma once

#include "JsonCodec.h"
#include "Side.h"

template <>
struct JsonCodec<Side>
{
    static nlohmann::json encode(const Side & object);
    static std::optional<Side> decode(const nlohmann::json & json);
};
