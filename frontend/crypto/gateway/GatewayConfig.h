#pragma once

#include "nlohmann/json_fwd.hpp"

#include <optional>
#include <string>
#include <string_view>

struct GatewayConfig
{
    struct Trading
    {
        std::string ws_url;
        std::string rest_url;
        std::string api_key;
        std::string secret_key;
    };

    struct MarketData
    {
        std::string ws_url;
        std::string rest_url;
    };

    std::string exchange;
    Trading trading;
    MarketData market_data;

    nlohmann::json to_json() const;
};

class GatewayConfigLoader
{
    static constexpr std::string_view s_config_env_var = "GATEWAY_CONFIG_DIR";

public:
    static std::optional<GatewayConfig> load();
};
