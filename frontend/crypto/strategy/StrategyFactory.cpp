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
#include "VolumeImbalanceTrendStrategy.h"

#include <fstream>
#include <optional>

#define BUILDER(Name)                 \
    {#Name,                           \
     [](const JsonStrategyConfig & c, \
        EventLoop & el,               \
        StrategyChannelsRefs ch,      \
        OrderManager & om) { return std::make_shared<Name##Strategy>(c, el, ch, om); }}

StrategyFactory::StrategyFactory()
{
    m_builders = {
            BUILDER(Mock),
            BUILDER(DoubleSma),
            BUILDER(BollingerBands),
            BUILDER(CandleBollingerBands),
            BUILDER(DebugEveryTick),
            BUILDER(RateOfChange),
            BUILDER(DSMADiff),
            BUILDER(Ratchet),
            BUILDER(RelativeStrengthIndex),
            BUILDER(BBRSI),
            BUILDER(DynamicGrid),
            BUILDER(DynamicGridAdx),
            BUILDER(StaticGrid),
            BUILDER(VolumeImbalanceTrend),
    };
}

StrategyFactory & StrategyFactory::i()
{
    static StrategyFactory f;
    return f;
}

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
        LOG_ERROR("Failed to open JSON file: {}", json_file_path);
        return {};
    }();
    return json_data;
}

std::vector<std::string> StrategyFactory::get_all_strategy_names() const
{
    std::vector<std::string> res;
    res.reserve(m_builders.size());
    for (const auto & [name, _] : m_builders) {
        res.emplace_back(name);
    }
    return res;
}

std::optional<std::shared_ptr<IStrategy>> StrategyFactory::build_strategy(
        const std::string & strategy_name,
        const JsonStrategyConfig & config,
        EventLoop & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders) const
{
    const auto it = m_builders.find(strategy_name);
    if (it == m_builders.end()) {
        LOG_ERROR("Unknown strategy name: {}", strategy_name);
        return std::nullopt;
    }

    const auto & [_, builder] = *it;
    return builder(config, event_loop, channels, orders);
}
