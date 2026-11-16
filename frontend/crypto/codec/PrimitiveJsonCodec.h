#pragma once

#include "JsonCodec.h"

#include <chrono>
#include <crossguid/guid.hpp>
#include <string>

template <>
struct JsonCodec<double>
{
    static nlohmann::json encode(const double & value);
    static std::optional<double> decode(const nlohmann::json & json);
};

template <>
struct JsonCodec<std::string>
{
    static nlohmann::json encode(const std::string & value);
    static std::optional<std::string> decode(const nlohmann::json & json);
};

template <>
struct JsonCodec<std::chrono::milliseconds>
{
    static nlohmann::json encode(const std::chrono::milliseconds & value);
    static std::optional<std::chrono::milliseconds> decode(const nlohmann::json & json);
};

template <>
struct JsonCodec<xg::Guid>
{
    static nlohmann::json encode(const xg::Guid & value);
    static std::optional<xg::Guid> decode(const nlohmann::json & json);
};
