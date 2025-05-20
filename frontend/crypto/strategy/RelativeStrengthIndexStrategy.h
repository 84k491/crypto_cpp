#pragma once

#include "JsonStrategyConfig.h"
#include "RelativeStrengthIndex.h"
#include "StrategyBase.h"

class RelativeStrengthIndexStrategyConfig
{
public:
    RelativeStrengthIndexStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_timeframe = {};
    unsigned m_interval = {};
    unsigned m_margin = 0;
};

class RelativeStrengthIndexStrategy : public StrategyBase
{
public:
    RelativeStrengthIndexStrategy(
            const RelativeStrengthIndexStrategyConfig & config,
            EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
            EventTimeseriesChannel<Candle> & candle_channel);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    std::optional<Signal> push_candle(const Candle & c);

private:
    RelativeStrengthIndexStrategyConfig m_config;
    RelativeStrengthIndex m_rsi;
};
