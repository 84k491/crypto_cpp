#include "ConfigGenerator.h"

OptimizerParser::OptimizerParser(OptimizerInputs optimizer_inputs)
    : m_inputs(std::move(optimizer_inputs))
{
}

std::vector<JsonStrategyConfig> OptimizerParser::get_possible_configs()
{
    const auto entry_configs = get_possible_configs(m_inputs.entry_strategy);

    std::vector<JsonStrategyConfig> outputs;
    outputs.reserve(entry_configs.size());
    for (const auto & entry_config : entry_configs) {
        outputs.emplace_back(entry_config);
    }
    return outputs;
}

std::vector<JsonStrategyConfig> OptimizerParser::get_possible_configs(const StrategyOptimizerInputs & strategy_optimizer_inputs)
{
    std::vector<JsonStrategyConfig> ouput_jsons;

    std::map<std::string, std::tuple<double, double, double>> limits_map;
    std::map<std::string, double> current_values_map;
    const auto params = strategy_optimizer_inputs.meta.get()["parameters"].get<std::vector<nlohmann::json>>();
    for (const auto & param : params) {
        const auto name = param["name"].get<std::string>();
        if (strategy_optimizer_inputs.optimizable_parameters.contains(name)) {
            const auto max_value = param["max_value"].get<double>();
            const auto step = param["step"].get<double>();
            const auto min_value = param["min_value"].get<double>();
            limits_map[name] = {min_value, step, max_value};
            current_values_map[name] = min_value;
        }
        else {
            const auto step = param["step"].get<double>();
            const auto min_value = strategy_optimizer_inputs.current_config.get()[name].get<double>();
            const auto max_value = min_value + step;
            limits_map[name] = {min_value, step, max_value};
            current_values_map[name] = min_value;
        }
    }

    const auto increment_current_param = [&] {
        for (auto & [param_name, param_value] : current_values_map) {
            const auto [min, step, max] = limits_map[param_name];
            param_value += step;
            if (param_value < max) {
                return true;
            }
            param_value = min;
        }
        return false;
    };

    for (bool f = true; f; f = increment_current_param()) {
        nlohmann::json json;
        for (const auto & [name, value] : current_values_map) {
            json[name] = value;
        }
        ouput_jsons.emplace_back(std::move(json));
    }

    return ouput_jsons;
}
