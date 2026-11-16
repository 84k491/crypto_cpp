#include "StartBacktestCommandJsonCodec.h"

#include "PrimitiveJsonCodec.h"

namespace {

std::string encode_playback_mode(PlaybackMode mode)
{
    switch (mode) {
    case PlaybackMode::Rapid: return "Rapid";
    case PlaybackMode::Playback: return "Playback";
    case PlaybackMode::Step: return "Step";
    }
    return "";
}

std::optional<PlaybackMode> decode_playback_mode(const std::string & text)
{
    for (const auto mode : {PlaybackMode::Rapid, PlaybackMode::Playback, PlaybackMode::Step}) {
        if (encode_playback_mode(mode) == text) {
            return mode;
        }
    }
    return std::nullopt;
}

} // namespace

nlohmann::json JsonCodec<StartBacktestCommand>::encode(const StartBacktestCommand & object)
{
    nlohmann::json json = {
            {"command_guid", JsonCodec<xg::Guid>::encode(object.command_guid)},
            {"symbol_name", object.symbol_name},
            {"strategy_name", object.strategy_name},
            {"strategy_config", object.strategy_config.get()},
            {"start", JsonCodec<std::chrono::milliseconds>::encode(object.start)},
            {"end", JsonCodec<std::chrono::milliseconds>::encode(object.end)},
            {"channel_capacity", nullptr},
            {"playback_mode", encode_playback_mode(object.playback_settings.mode)},
            {"playback_delta", JsonCodec<std::chrono::milliseconds>::encode(object.playback_settings.delta)},
    };
    if (object.channel_capacity.has_value()) {
        json["channel_capacity"] = JsonCodec<std::chrono::milliseconds>::encode(object.channel_capacity.value());
    }
    return json;
}

std::optional<StartBacktestCommand> JsonCodec<StartBacktestCommand>::decode(const nlohmann::json & json)
{
    try {
        std::optional<std::chrono::milliseconds> channel_capacity;
        if (!json.at("channel_capacity").is_null()) {
            channel_capacity = JsonCodec<std::chrono::milliseconds>::decode(json.at("channel_capacity")).value();
        }

        return StartBacktestCommand{
                .command_guid = JsonCodec<xg::Guid>::decode(json.at("command_guid")).value(),
                .symbol_name = json.at("symbol_name").get<std::string>(),
                .strategy_name = json.at("strategy_name").get<std::string>(),
                .strategy_config = JsonStrategyConfig{json.at("strategy_config")},
                .start = JsonCodec<std::chrono::milliseconds>::decode(json.at("start")).value(),
                .end = JsonCodec<std::chrono::milliseconds>::decode(json.at("end")).value(),
                .channel_capacity = channel_capacity,
                .playback_settings = PlaybackSettingsEvent{
                        decode_playback_mode(json.at("playback_mode").get<std::string>()).value(),
                        JsonCodec<std::chrono::milliseconds>::decode(json.at("playback_delta")).value(),
                },
        };
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
