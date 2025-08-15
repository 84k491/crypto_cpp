#include "StrategyFactory.h"

#include "BBRSIStrategy.h"
#include "BollingerBandsStrategy.h"
#include "CandleBollingerBandsStrategy.h"
#include "DSMADiffStrategy.h"
#include "DebugEveryTickStrategy.h"
#include "DoubleSmaStrategy.h"
#include "DynamicGridAdxStrategy.h"
#include "DynamicGridStrategy.h"
#include "Logger.h"
#include "MockStrategy.h"
#include "RatchetStrategy.h"
#include "RateOfChangeStrategy.h"
#include "RelativeStrengthIndexStrategy.h"
#include "StaticGridStrategy.h"
#include "StrategyInterface.h"

#include <fstream>
#include <optional>

StrategyFactory::StrategyFactory()
{
    m_builders = {
            {"Mock",
             [](const JsonStrategyConfig &,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<MockStrategy>(el, ch, om); }},

            {"DoubleSma",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<DoubleSmaStrategy>(c, el, ch, om); }},

            {"BollingerBands",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<BollingerBandsStrategy>(c, el, ch, om); }},

            {"CandleBB",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<CandleBollingerBandsStrategy>(c, el, ch, om); }},

            {"DebugEveryTick",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<DebugEveryTickStrategy>(c, el, ch, om); }},

            {"RateOfChange",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<RateOfChangeStrategy>(c, el, ch, om); }},

            {"DSMADiff",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<DSMADiffStrategy>(c, el, ch, om); }},

            {"Ratchet",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<RatchetStrategy>(c, el, ch, om); }},

            {"RelativeStrengthIndex",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<RelativeStrengthIndexStrategy>(c, el, ch, om); }},

            {"BBRSI",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<BBRSIStrategy>(c, el, ch, om); }},

            {"DynamicGrid",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<DynamicGridStrategy>(c, el, ch, om); }},

            {"DynamicGridAdx",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<DynamicGridAdxStrategy>(c, el, ch, om); }},

            {"StaticGrid",
             [](const JsonStrategyConfig & c,
                EventLoopSubscriber & el,
                StrategyChannelsRefs ch,
                OrderManager & om) { return std::make_shared<StaticGridStrategy>(c, el, ch, om); }},
    };
}

StrategyFactory & StrategyFactory::i()
{
    static StrategyFactory f;
    return f;
}

std::optional<JsonStrategyMetaInfo> StrategyFactory::get_meta_info(const std::string & strategy_name) const
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

std::vector<std::string> StrategyFactory::get_all_strategy_names() const
{
    std::vector<std::string> res;
    for (const auto & [name, _] : m_builders) {
        res.emplace_back(name);
    }
    return res;
}

std::optional<std::shared_ptr<IStrategy>> StrategyFactory::build_strategy(
        const std::string & strategy_name,
        const JsonStrategyConfig & config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders) const
{
    const auto it = m_builders.find(strategy_name);
    if (it == m_builders.end()) {
        Logger::logf<LogLevel::Error>("Unknown strategy name: {}", strategy_name);
        return {};
    }

    const auto & [_, builder] = *it;
    return builder(config, event_loop, channels, orders);
}
