#include "CommandReplyJsonCodec.h"

#include "PrimitiveJsonCodec.h"

nlohmann::json JsonCodec<CommandReply>::encode(const CommandReply & object)
{
    nlohmann::json json = {
            {"command_guid", JsonCodec<xg::Guid>::encode(object.command_guid)},
            {"error", nullptr},
    };
    if (object.error.has_value()) {
        json["error"] = object.error.value();
    }
    return json;
}

std::optional<CommandReply> JsonCodec<CommandReply>::decode(const nlohmann::json & json)
{
    try {
        CommandReply reply{.command_guid = JsonCodec<xg::Guid>::decode(json.at("command_guid")).value(), .error = std::nullopt};
        if (!json.at("error").is_null()) {
            reply.error = json.at("error").get<std::string>();
        }
        return reply;
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
