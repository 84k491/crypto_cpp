#include "StopCommandJsonCodec.h"

#include "PrimitiveJsonCodec.h"

nlohmann::json JsonCodec<StopCommand>::encode(const StopCommand & object)
{
    return {
            {"command_guid", JsonCodec<xg::Guid>::encode(object.command_guid)},
    };
}

std::optional<StopCommand> JsonCodec<StopCommand>::decode(const nlohmann::json & json)
{
    try {
        return StopCommand{
                .command_guid = JsonCodec<xg::Guid>::decode(json.at("command_guid")).value(),
        };
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
