#include "GatewayConfig.h"

#include "LogLevel.h"
#include "Logger.h"
#include "nlohmann/json_fwd.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>

void from_json(const json & j, GatewayConfig::Trading & config)
{
    j.at("ws_url").get_to(config.ws_url);
    j.at("rest_url").get_to(config.rest_url);
    j.at("api_key").get_to(config.api_key);
    j.at("secret_key").get_to(config.secret_key);
}

void from_json(const json & j, GatewayConfig::MarketData & config)
{
    j.at("ws_url").get_to(config.ws_url);
    j.at("rest_url").get_to(config.rest_url);
}

void from_json(const json & j, GatewayConfig & config)
{
    j.at("exchange").get_to(config.exchange);
    j.at("trading").get_to(config.trading);
    j.at("market_data").get_to(config.market_data);
}

nlohmann::json GatewayConfig::to_json() const
{
    nlohmann::json j = {
        {"exchange", exchange},
        {"trading", {
            {"ws_url", trading.ws_url},
            {"rest_url", trading.rest_url},
            {"api_key", trading.api_key},
            {"secret_key", trading.secret_key},
        }},
        {"market_data", {
            {"ws_url", market_data.ws_url},
            {"rest_url", market_data.rest_url},
        }},
    };
    return j;
}

std::optional<GatewayConfig> GatewayConfigLoader::load()
{
    namespace fs = std::filesystem;

    const auto env_value = std::getenv(s_config_env_var.data());
    if (!env_value) {
        Logger::logf<LogLevel::Warning>("Environment variable {} is not set", s_config_env_var);
        return {};
    }
    const std::string config_dir = env_value;

    if (config_dir.empty()) {
        Logger::logf<LogLevel::Warning>("Environment variable {} is empty", s_config_env_var);
        return {};
    }

    std::map<std::string, nlohmann::json> sorted_configs;
    for (const auto & file : fs::directory_iterator(config_dir)) {
        const std::string filename = file.path().filename();
        if (filename.length() < 5 || filename.substr(filename.length() - 5) != ".json") {
            Logger::logf<LogLevel::Warning>("Not a JSON file in config dir: {}", filename);
            continue;
        }

        std::ifstream ifs(file.path());
        const auto json = json::parse(ifs);
        if (!json.is_object()) {
            Logger::logf<LogLevel::Warning>("Not a JSON object in config dir: {}", filename);
            continue;
        }
        sorted_configs.emplace(filename, json);
    }

    if (sorted_configs.empty()) {
        return {};
    }
    const auto & [filename, json] = *sorted_configs.begin();

    Logger::logf<LogLevel::Info>("Loading config from file: {}", filename);

    GatewayConfig config;
    from_json(json, config);
    Logger::logf<LogLevel::Debug>("Gateway config loaded: {}", config.to_json().dump(2));

    return config;
}
