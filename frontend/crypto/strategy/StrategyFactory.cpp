#include "StrategyFactory.h"

#include "BBRSIStrategy.h"
#include "GridAdxStrategy.h"
#include "GridStrategy.h"
#include "BollingerBandsStrategy.h"
#include "CandleBollingerBandsStrategy.h"
#include "DSMADiffStrategy.h"
#include "DebugEveryTickStrategy.h"
#include "DoubleSmaStrategy.h"
#include "Logger.h"
#include "MockStrategy.h"
#include "RatchetStrategy.h"
#include "RateOfChangeStrategy.h"
#include "RelativeStrengthIndexStrategy.h"
#include "StrategyInterface.h"

#include <fstream>
#include <optional>

std::optional<JsonStrategyMetaInfo> StrategyFactory::get_meta_info(const std::string & strategy_name)
{
    static constexpr std::string_view strategy_parameters_dir = "strategy_parameters/";
    const auto name_to_filename = [&](const std::string & name) {
        return name + ".json";
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
        const JsonStrategyConfig & config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
{
    if (strategy_name == "Mock") {
        std::shared_ptr<IStrategy> res = std::make_shared<MockStrategy>(
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "DoubleSma") {
        std::shared_ptr<IStrategy> res = std::make_shared<DoubleSmaStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "BollingerBands") {
        std::shared_ptr<IStrategy> res = std::make_shared<BollingerBandsStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "CandleBB") {
        std::shared_ptr<IStrategy> res = std::make_shared<CandleBollingerBandsStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "DebugEveryTick") {
        std::shared_ptr<IStrategy> res = std::make_shared<DebugEveryTickStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "RateOfChange") {
        std::shared_ptr<IStrategy> res = std::make_shared<RateOfChangeStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "DSMADiff") {
        std::shared_ptr<IStrategy> res = std::make_shared<DSMADiffStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "Ratchet") {
        std::shared_ptr<IStrategy> res = std::make_shared<RatchetStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "RelativeStrengthIndex") {
        std::shared_ptr<IStrategy> res = std::make_shared<RelativeStrengthIndexStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "BBRSI") {
        std::shared_ptr<IStrategy> res = std::make_shared<BBRSIStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "Grid") {
        std::shared_ptr<IStrategy> res = std::make_shared<GridStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    if (strategy_name == "GridAdx") {
        std::shared_ptr<IStrategy> res = std::make_shared<GridAdxStrategy>(
                config,
                event_loop,
                channels,
                orders);
        return res;
    }
    Logger::logf<LogLevel::Error>("Unknown strategy name: {}", strategy_name);
    return {};
}
