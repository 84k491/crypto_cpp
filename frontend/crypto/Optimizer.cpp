#include "Optimizer.h"

#include "BacktestTradingGateway.h"
#include "JsonStrategyConfig.h"
#include "StrategyFactory.h"
#include "StrategyInstance.h"
#include "TpslExitStrategy.h"

#include <iostream>
#include <vector>

std::vector<std::pair<nlohmann::json, TpslExitStrategyConfig>> OptimizerParser::get_possible_configs()
{
    const auto entry_configs = [&]() -> std::vector<nlohmann::json> {
        if (const auto * meta = std::get_if<JsonStrategyMetaInfo>(&m_inputs.entry_strategy); meta != nullptr) {
            return get_possible_configs(*meta);
        }
        if (const auto * config = std::get_if<JsonStrategyConfig>(&m_inputs.entry_strategy); config != nullptr) {
            const auto& json = config->get();
            return {json};
        }
        return {};
    }();
    const auto exit_configs = get_possible_exit_configs();

    std::vector<std::pair<nlohmann::json, TpslExitStrategyConfig>> outputs;
    for (const auto & entry_config : entry_configs) {
        for (const auto & exit_config : exit_configs) {
            outputs.emplace_back(entry_config, exit_config);
        }
    }
    return outputs;
}

std::vector<TpslExitStrategyConfig> OptimizerParser::get_possible_exit_configs()
{
    if (const auto * meta = std::get_if<JsonStrategyMetaInfo>(&m_inputs.exit_strategy);
        meta != nullptr) {
        const auto exit_jsons = get_possible_configs(*meta);
        std::vector<TpslExitStrategyConfig> outputs;
        outputs.reserve(exit_jsons.size());
        for (const auto & exit_json : exit_jsons) {
            outputs.emplace_back(exit_json);
        }
        return outputs;
    }
    if (const auto * config = std::get_if<TpslExitStrategyConfig>(&m_inputs.exit_strategy);
        config != nullptr) {
        return {*config};
    }
    std::cout << "ERROR Invalid exit strategy" << std::endl;
    return {};
}

std::vector<nlohmann::json> OptimizerParser::get_possible_configs(const JsonStrategyMetaInfo & meta_info)
{
    if (const auto strategy_name = meta_info.get().find("strategy_name");
        strategy_name != meta_info.get().end()) {
        std::cout << "Reading JSON file for " << strategy_name->get<std::string>() << std::endl;
    }

    std::vector<nlohmann::json> ouput_jsons;

    std::map<std::string, std::tuple<double, double, double>> limits_map;
    std::map<std::string, double> current_values_map;
    const auto params = meta_info.get()["parameters"].get<std::vector<nlohmann::json>>();
    for (const auto & param : params) {
        const auto name = param["name"].get<std::string>();
        const auto max_value = param["max_value"].get<double>();
        const auto step = param["step"].get<double>();
        const auto min_value = param["min_value"].get<double>();
        limits_map[name] = {min_value, step, max_value};
        current_values_map[name] = min_value;
    }

    const auto increment_current_param = [&] {
        for (auto & [param_name, param_value] : current_values_map) {
            const auto [min, step, max] = limits_map[param_name];
            if (param_value < max) {
                param_value += step;
                return true;
            }
            param_value = min;
        }
        return false;
    };

    while (increment_current_param()) {
        nlohmann::json json;
        for (const auto & [name, value] : current_values_map) {
            json[name] = value;
        }
        ouput_jsons.emplace_back(std::move(json));
    }

    return ouput_jsons;
}

std::optional<std::pair<JsonStrategyConfig, TpslExitStrategyConfig>> Optimizer::optimize()
{
    OptimizerParser parser(m_optimizer_inputs);

    double max_profit = -std::numeric_limits<double>::max();
    const auto configs = parser.get_possible_configs();
    std::optional<decltype(configs)::value_type> best_config;
    for (unsigned i = 0; i < configs.size(); ++i) {
        const auto & [entry_config, exit_config] = configs[i];
        const auto strategy_opt = StrategyFactory::build_strategy(m_strategy_name, entry_config);
        if (!strategy_opt.has_value() || !strategy_opt.value() || !strategy_opt.value()->is_valid()) {
            continue;
        }

        const auto md_request = [&]() {
            MarketDataRequest result;
            result.go_live = false;
            result.historical_range = {m_timerange.start(), m_timerange.end()};
            return result;
        }();

        BacktestTradingGateway tr_gateway;
        StrategyInstance strategy_instance(
                m_symbol,
                md_request,
                strategy_opt.value(),
                exit_config,
                m_gateway,
                tr_gateway);
        tr_gateway.set_price_source(strategy_instance.klines_publisher());
        strategy_instance.run_async();
        strategy_instance.wait_for_finish().wait();
        const auto profit = strategy_instance.strategy_result_publisher().get().final_profit;
        if (max_profit < profit) {
            max_profit = profit;
            best_config = configs[i];
        }
        m_on_passed_check(i, configs.size());
    }
    if (!best_config.has_value()) {
        return {};
    }
    m_on_passed_check(configs.size(), configs.size());
    return {{JsonStrategyConfig{best_config.value().first}, best_config.value().second}};
}
