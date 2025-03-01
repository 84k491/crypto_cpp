#include "StrategyFactory.h"

#include "BollingerBandsStrategy.h"
#include "DSMADiffStrategy.h"
#include "DebugEveryTickStrategy.h"
#include "DoubleSmaStrategy.h"
#include "Logger.h"
#include "StrategyInterface.h"
#include "TunedBollingerBandsStrategy.h"
#include "RateOfChangeStrategy.h"

#include <fstream>
#include <optional>

std::optional<JsonStrategyMetaInfo> StrategyFactory::get_meta_info(const std::string & strategy_name)
{
    static constexpr std::string_view strategy_parameters_dir = "strategy_parameters/";
    const auto name_to_filename = [&](const std::string &name) {
        return name+ ".json";
    };

    std::string json_file_path = "./";
    json_file_path += strategy_parameters_dir;
    json_file_path += name_to_filename(strategy_name);

    const auto json_data = [&]() -> std::optional<JsonStrategyMetaInfo> {
        std::ifstream file(json_file_path);
        if (file.is_open()) {
            nlohmann::json json_data;
            file >> json_data;
            file.close();
            return json_data;
        }
        Logger::logf<LogLevel::Error>("Failed to open JSON file: {}", json_file_path);
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
    if (strategy_name == "BollingerBands") {
        std::shared_ptr<IStrategy> res = std::make_shared<BollingerBandsStrategy>(config);
        return res;
    }
    if (strategy_name == "TunedBB") {
        std::shared_ptr<IStrategy> res = std::make_shared<TunedBollingerBandsStrategy>(config);
        return res;
    }
    if (strategy_name == "DebugEveryTick") {
        std::shared_ptr<IStrategy> res = std::make_shared<DebugEveryTickStrategy>(config);
        return res;
    }
    if (strategy_name == "RateOfChange") {
        std::shared_ptr<IStrategy> res = std::make_shared<RateOfChangeStrategy>(config);
        return res;
    }
    if (strategy_name == "DSMADiff") {
        std::shared_ptr<IStrategy> res = std::make_shared<DSMADiffStrategy>(config);
        return res;
    }
    Logger::logf<LogLevel::Error>("Unknown strategy name: {}", strategy_name);
    return {};
}
