#pragma once

#include <compare>
#include <string>
#include <vector>

struct ChartSeriesName
{
    std::string chart_name;
    std::string series_name;

    auto operator<=>(const ChartSeriesName &) const = default;
};

struct ChartSeriesList
{
    std::vector<ChartSeriesName> series_names;
};

namespace FixedChartSeries {
inline const ChartSeriesName candles{.chart_name = "prices", .series_name = "candles"};
inline const ChartSeriesName trades{.chart_name = "prices", .series_name = "trades"};
inline const ChartSeriesName price_levels{.chart_name = "prices", .series_name = "price_levels"};
inline const ChartSeriesName tpsl{.chart_name = "prices", .series_name = "tpsl"};
inline const ChartSeriesName trailing_stop{.chart_name = "prices", .series_name = "trailing_stop"};
inline const ChartSeriesName market_states{.chart_name = "prices", .series_name = "market_states"};
inline const ChartSeriesName depo{.chart_name = "depo", .series_name = "depo"};
} // namespace FixedChartSeries
