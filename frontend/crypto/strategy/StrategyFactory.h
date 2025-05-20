#pragma once

#include "Candle.h"
#include "JsonStrategyConfig.h"
#include "StrategyInterface.h"

#include <memory>

class StrategyFactory
{
public:
    StrategyFactory() = default;

    static std::optional<JsonStrategyMetaInfo> get_meta_info(
            const std::string & strategy_name);
    static std::optional<std::shared_ptr<IStrategy>> build_strategy(
            const std::string & strategy_name,
            const JsonStrategyConfig & config,
            EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
            EventTimeseriesChannel<double> & price_channel,
            EventTimeseriesChannel<Candle> & candle_channel);
};
