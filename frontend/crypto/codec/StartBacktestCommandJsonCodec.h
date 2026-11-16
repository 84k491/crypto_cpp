#pragma once

#include "BackendCommands.h"
#include "JsonCodec.h"

template <>
struct JsonCodec<StartBacktestCommand>
{
    static nlohmann::json encode(const StartBacktestCommand & object);
    static std::optional<StartBacktestCommand> decode(const nlohmann::json & json);
};
