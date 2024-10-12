#include "GatewayConfig.h"

#include "Logger.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

void from_json(const json & j, GatewayConfig & config)
{
    j.at("name").get_to(config.name);

    j.at("exchange").get_to(config.exchange);
    j.at("type").get_to(config.type);

    j.at("ws_url").get_to(config.ws_url);
    j.at("rest_url").get_to(config.rest_url);
    j.at("api_key").get_to(config.api_key);
    j.at("secret_key").get_to(config.secret_key);
}

std::optional<GatewayConfig> GatewayConfigLoader::load(const std::string & exchange, const std::string & type, const std::string & name)
{
    namespace fs = std::filesystem;

    const std::string config_dir = std::getenv(std::string(s_config_env_var).c_str());
    if (config_dir.empty()) {
        Logger::logf<LogLevel::Warning>("Environment variable {} is not set", s_config_env_var);
        return {};
    }

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

        GatewayConfig config;
        from_json(json, config);

        if (config.exchange == exchange && config.type == type && config.name == name) {
            return config;
        }
    }

    return {};
}
