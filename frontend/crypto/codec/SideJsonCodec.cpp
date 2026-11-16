#include "SideJsonCodec.h"

nlohmann::json JsonCodec<Side>::encode(const Side & object)
{
    return object.to_string();
}

std::optional<Side> JsonCodec<Side>::decode(const nlohmann::json & json)
{
    try {
        const auto text = json.get<std::string>();
        for (const auto & side : {Side::buy(), Side::sell()}) {
            if (side.to_string() == text) {
                return side;
            }
        }
        return std::nullopt;
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
