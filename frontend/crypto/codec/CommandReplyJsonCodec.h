#pragma once

#include "BackendCommands.h"
#include "JsonCodec.h"

template <>
struct JsonCodec<CommandReply>
{
    static nlohmann::json encode(const CommandReply & object);
    static std::optional<CommandReply> decode(const nlohmann::json & json);
};
