#pragma once

#include "JsonCodec.h"
#include "RemoteMessages.h"

template <>
struct JsonCodec<RemoteRequest>
{
    static nlohmann::json encode(const RemoteRequest & object);
    static std::optional<RemoteRequest> decode(const nlohmann::json & json);
};

template <>
struct JsonCodec<RemoteResponse>
{
    static nlohmann::json encode(const RemoteResponse & object);
    static std::optional<RemoteResponse> decode(const nlohmann::json & json);
};
