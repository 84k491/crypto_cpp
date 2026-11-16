#pragma once

#include "JsonCodec.h"
#include "PrimitiveJsonCodec.h"

#include <chrono>
#include <utility>

template <class ObjectT>
struct JsonCodec<std::pair<std::chrono::milliseconds, ObjectT>>
{
    static nlohmann::json encode(const std::pair<std::chrono::milliseconds, ObjectT> & point)
    {
        return {
                {"ts", JsonCodec<std::chrono::milliseconds>::encode(point.first)},
                {"value", JsonCodec<ObjectT>::encode(point.second)},
        };
    }

    static std::optional<std::pair<std::chrono::milliseconds, ObjectT>> decode(const nlohmann::json & json)
    {
        try {
            return std::pair<std::chrono::milliseconds, ObjectT>{
                    JsonCodec<std::chrono::milliseconds>::decode(json.at("ts")).value(),
                    JsonCodec<ObjectT>::decode(json.at("value")).value(),
            };
        }
        catch (const std::exception &) {
            return std::nullopt;
        }
    }
};
