#pragma once

#include "ChartSeriesList.h"
#include "JsonCodec.h"

template <>
struct JsonCodec<ChartSeriesList>
{
    static nlohmann::json encode(const ChartSeriesList & object);
    static std::optional<ChartSeriesList> decode(const nlohmann::json & json);
};
