#include "PrimitiveJsonCodec.h"

#include <limits>

nlohmann::json JsonCodec<double>::encode(const double & value)
{
    return value;
}

std::optional<double> JsonCodec<double>::decode(const nlohmann::json & json)
{
    try {
        if (json.is_null()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return json.get<double>();
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}

nlohmann::json JsonCodec<std::string>::encode(const std::string & value)
{
    return value;
}

std::optional<std::string> JsonCodec<std::string>::decode(const nlohmann::json & json)
{
    try {
        return json.get<std::string>();
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}

nlohmann::json JsonCodec<std::chrono::milliseconds>::encode(const std::chrono::milliseconds & value)
{
    return value.count();
}

std::optional<std::chrono::milliseconds> JsonCodec<std::chrono::milliseconds>::decode(const nlohmann::json & json)
{
    try {
        return std::chrono::milliseconds{json.get<int64_t>()};
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}

nlohmann::json JsonCodec<xg::Guid>::encode(const xg::Guid & value)
{
    return value.str();
}

std::optional<xg::Guid> JsonCodec<xg::Guid>::decode(const nlohmann::json & json)
{
    try {
        return xg::Guid{json.get<std::string>()};
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
