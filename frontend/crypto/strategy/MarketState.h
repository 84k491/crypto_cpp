#pragma once

#include <string>

enum class MarketState
{
    None,
    NoTrend,
    UpTrend,
    DownTrend,
    HighVolatility,
};

template <typename EnumT>
std::string enumToString(EnumT v);

template <>
inline std::string enumToString(MarketState v)
{
    switch (v) {
    case MarketState::None: return "None";
    case MarketState::NoTrend: return "NoTrend";
    case MarketState::UpTrend: return "UpTrend";
    case MarketState::DownTrend: return "DownTrend";
    case MarketState::HighVolatility: return "HighVolatility";
    }

    return "";
};

struct MarketStateRenderObject
{
    double upper_limit;
    double lower_limit;

    MarketState state;
};
