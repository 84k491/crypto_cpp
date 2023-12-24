#include "Optimizer.h"

#include <iostream>

std::vector<nlohmann::json> OptimizerParser::get_possible_configs()
{
    if (const auto strategy_name = m_data.get().find("strategy_name"); strategy_name != m_data.get().end()) {
        std::cout << "Reading JSON file for " << strategy_name->get<std::string>() << std::endl;
    }

    std::vector<nlohmann::json> ouput_jsons;

    std::map<std::string, std::tuple<double, double, double>> limits_map;
    std::map<std::string, double> current_values_map;
    const auto params = m_data.get()["parameters"].get<std::vector<nlohmann::json>>();
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

std::string OptimizerParser::get_strategy_name() const
{
    if (!m_data.get().contains("strategy_name")) {
        std::cout << "ERROR No strategy name in JSON file" << std::endl;
        return "";
    }
    return m_data.get()["strategy_name"].get<std::string>();
}
