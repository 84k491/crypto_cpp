#pragma once

#include "JsonStrategyConfig.h"
#include "RelativeStrengthIndex.h"
#include "StrategyInterface.h"

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

class RelativeStrengthIndexStrategy : public IStrategy
{
public:
    RelativeStrengthIndexStrategy(
            const RelativeStrengthIndexStrategyConfig & config,
            EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
            EventTimeseriesChannel<Candle> & candle_channel);

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;
    EventTimeseriesChannel<Signal> & signal_channel() override;

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    std::optional<Signal> push_candle(const Candle & c);

private:
    RelativeStrengthIndexStrategyConfig m_config;
    RelativeStrengthIndex m_rsi;
    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
    EventTimeseriesChannel<Signal> m_signal_channel;

    std::list<std::shared_ptr<ISubscription>> m_channel_subs;
};
