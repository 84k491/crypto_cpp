#pragma once

#include "ChartSeriesList.h"

#include <cstdint>
#include <string>

inline constexpr uint16_t remote_channel_port = 9977;

namespace RemoteChannelNames {
inline const std::string strategy_names = "strategy_names";
inline const std::string symbols = "symbols";
inline const std::string start_backtest = "start_backtest";
inline const std::string stop = "stop";
inline const std::string command_replies = "command_replies";
inline const std::string work_status = "work_status";
inline const std::string strategy_result = "strategy_result";
inline const std::string positions = "positions";
inline const std::string chart_series_list = "chart_series_list";

inline std::string series(const ChartSeriesName & name)
{
    return "series/" + name.chart_name + "/" + name.series_name;
}
} // namespace RemoteChannelNames
