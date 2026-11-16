#pragma once

#include "Events.h"
#include "JsonStrategyConfig.h"

#include <chrono>
#include <crossguid/guid.hpp>
#include <optional>
#include <string>

struct StartBacktestCommand
{
    xg::Guid command_guid;
    std::string symbol_name;
    std::string strategy_name;
    JsonStrategyConfig strategy_config;
    std::chrono::milliseconds start;
    std::chrono::milliseconds end;
    std::optional<std::chrono::milliseconds> channel_capacity;
    PlaybackSettingsEvent playback_settings;
};

struct StartLiveRunCommand
{
    xg::Guid command_guid;
    std::string symbol_name;
    std::string strategy_name;
    JsonStrategyConfig strategy_config;
    std::optional<std::chrono::milliseconds> channel_capacity;
};

struct StopCommand
{
    xg::Guid command_guid;
};

struct CommandReply
{
    xg::Guid command_guid;
    std::optional<std::string> error;
};
