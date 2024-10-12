#pragma once

#include <string>
#include <string_view>
#include <optional>

struct GatewayConfig
{
    std::string name; // testnet/production

    std::string exchange;
    std::string type; // md/tr

    std::string ws_url;
    std::string rest_url;
    std::string api_key;
    std::string secret_key;
};

class GatewayConfigLoader
{
    static constexpr std::string_view s_config_env_var = "GATEWAY_CONFIG_DIR";
public:
    static std::optional<GatewayConfig> load(const std::string & exchange, const std::string & type, const std::string & name);
};
