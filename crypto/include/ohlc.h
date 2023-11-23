#pragma once

#include <nlohmann/json.hpp>

#include <chrono>

struct OHLC
{
    std::chrono::milliseconds timestamp{};
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
    double turnover{};
};

using json = nlohmann::json;
void to_json(json & j, const OHLC & ohlc);
std::ostream & operator<<(std::ostream & os, const OHLC & ohlc);
