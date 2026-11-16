#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace RemoteOperation {
inline const std::string get = "get";
inline const std::string data_copy = "data_copy";
inline const std::string data_copy_in_range = "data_copy_in_range";
inline const std::string subscribe = "subscribe";
inline const std::string unsubscribe = "unsubscribe";
inline const std::string push = "push";
} // namespace RemoteOperation

namespace RemoteResponseKind {
inline const std::string reply = "reply";
inline const std::string subscribed = "subscribed";
inline const std::string snapshot = "snapshot";
inline const std::string update = "update";
inline const std::string closed = "closed";
inline const std::string error = "error";
} // namespace RemoteResponseKind

struct RemoteRequest
{
    uint64_t request_id = 0;
    std::string channel_name;
    std::string operation;
    nlohmann::json arguments;
};

struct RemoteResponse
{
    uint64_t request_id = 0;
    std::string kind;
    nlohmann::json data;
};
