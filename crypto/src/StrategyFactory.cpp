#include "StrategyFactory.h"

#include "DoubleSmaStrategy.h"
#include "StrategyInterface.h"

#include <fstream>
#include <iostream>
#include <optional>

std::optional<JsonStrategyMetaInfo> StrategyFactory::get_meta_info(const std::string & strategy_name)
{
    static constexpr std::string_view strategy_parameters_dir = "strategy_parameters/";
    static const std::map<std::string, std::string> name_to_filename = {
            {
                    "DoubleSma",
                    "DoubleSma.json",
            },
    };

    if (name_to_filename.find(strategy_name) == name_to_filename.end()) {
        std::cout << "ERROR: Unknown strategy name: " << strategy_name << std::endl;
        return {};
    }
    std::string json_file_path = "./";
    json_file_path += strategy_parameters_dir;
    json_file_path += name_to_filename.at(strategy_name);

    const auto json_data = [&]() -> std::optional<JsonStrategyMetaInfo> {
        std::ifstream file(json_file_path);
        if (file.is_open()) {
            nlohmann::json json_data;
            file >> json_data;
            file.close();
            return json_data;
        }
        std::cout << "ERROR: Failed to open JSON file: " << json_file_path << std::endl;
        return {};
    }();
    return json_data;
}

std::optional<std::shared_ptr<IStrategy>> StrategyFactory::build_strategy(
        const std::string & strategy_name,
        const JsonStrategyConfig & config)
{
    if (strategy_name == "DoubleSma") {
        std::shared_ptr<IStrategy> res = std::make_shared<DoubleSmaStrategy>(config);
        return res;
    }
    std::cout << "ERROR: Unknown strategy name: " << strategy_name << std::endl;
    return {};
}
