#pragma once

#include "JsonCodec.h"
#include "Symbol.h"

template <>
struct JsonCodec<Symbol>
{
    static nlohmann::json encode(const Symbol & object);
    static std::optional<Symbol> decode(const nlohmann::json & json);
};
