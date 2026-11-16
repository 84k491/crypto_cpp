#include "RemoteMessagesJsonCodec.h"

nlohmann::json JsonCodec<RemoteRequest>::encode(const RemoteRequest & object)
{
    return {
            {"request_id", object.request_id},
            {"channel", object.channel_name},
            {"operation", object.operation},
            {"arguments", object.arguments},
    };
}

std::optional<RemoteRequest> JsonCodec<RemoteRequest>::decode(const nlohmann::json & json)
{
    try {
        return RemoteRequest{
                .request_id = json.at("request_id").get<uint64_t>(),
                .channel_name = json.at("channel").get<std::string>(),
                .operation = json.at("operation").get<std::string>(),
                .arguments = json.at("arguments"),
        };
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}

nlohmann::json JsonCodec<RemoteResponse>::encode(const RemoteResponse & object)
{
    return {
            {"request_id", object.request_id},
            {"kind", object.kind},
            {"data", object.data},
    };
}

std::optional<RemoteResponse> JsonCodec<RemoteResponse>::decode(const nlohmann::json & json)
{
    try {
        return RemoteResponse{
                .request_id = json.at("request_id").get<uint64_t>(),
                .kind = json.at("kind").get<std::string>(),
                .data = json.at("data"),
        };
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
