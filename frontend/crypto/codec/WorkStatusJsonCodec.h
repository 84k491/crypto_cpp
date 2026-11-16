#pragma once

#include "JsonCodec.h"
#include "WorkStatus.h"

template <>
struct JsonCodec<WorkStatus>
{
    static nlohmann::json encode(const WorkStatus & object);
    static std::optional<WorkStatus> decode(const nlohmann::json & json);
};
