#pragma once

enum MarketState
{
    None,
    NoTrend,
    UpTrend,
    DownTrend,
    HighVolatility,
};

struct MarketStateRenderObject
{
    double upper_limit;
    double lower_limit;

    MarketState state;
};
