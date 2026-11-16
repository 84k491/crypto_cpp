#pragma once

#include "BackendCommands.h"
#include "JsonCodec.h"

template <>
struct JsonCodec<StopCommand>
{
    static nlohmann::json encode(const StopCommand & object);
    static std::optional<StopCommand> decode(const nlohmann::json & json);
};
