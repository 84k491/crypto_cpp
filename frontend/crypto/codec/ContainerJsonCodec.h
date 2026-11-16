#pragma once

#include "JsonCodec.h"

#include <deque>
#include <vector>

template <class ObjectT>
struct JsonCodec<std::vector<ObjectT>>
{
    static nlohmann::json encode(const std::vector<ObjectT> & objects)
    {
        auto json = nlohmann::json::array();
        for (const auto & object : objects) {
            json.push_back(JsonCodec<ObjectT>::encode(object));
        }
        return json;
    }

    static std::optional<std::vector<ObjectT>> decode(const nlohmann::json & json)
    {
        std::vector<ObjectT> objects;
        for (const auto & element : json) {
            const auto object_opt = JsonCodec<ObjectT>::decode(element);
            if (!object_opt.has_value()) {
                return std::nullopt;
            }
            objects.push_back(object_opt.value());
        }
        return objects;
    }
};

template <class ObjectT>
struct JsonCodec<std::deque<ObjectT>>
{
    static nlohmann::json encode(const std::deque<ObjectT> & objects)
    {
        auto json = nlohmann::json::array();
        for (const auto & object : objects) {
            json.push_back(JsonCodec<ObjectT>::encode(object));
        }
        return json;
    }

    static std::optional<std::deque<ObjectT>> decode(const nlohmann::json & json)
    {
        std::deque<ObjectT> objects;
        for (const auto & element : json) {
            const auto object_opt = JsonCodec<ObjectT>::decode(element);
            if (!object_opt.has_value()) {
                return std::nullopt;
            }
            objects.push_back(object_opt.value());
        }
        return objects;
    }
};
