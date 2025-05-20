#pragma once

#include "Candle.h"
#include "JsonStrategyConfig.h"
#include "Ratchet.h"
#include "StrategyInterface.h"

class RatchetStrategyConfig
{
public:
    RatchetStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    double m_retracement;
    std::chrono::milliseconds m_timeframe;
};

class RatchetStrategy : public IStrategy
{
public:
    RatchetStrategy(
            RatchetStrategyConfig config,
            EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
            EventTimeseriesChannel<Candle> & candle_channel);

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;
    EventTimeseriesChannel<Signal> & signal_channel() override;

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    std::optional<Signal> push_candle(const Candle &);

private:
    RatchetStrategyConfig m_config;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;

    EventTimeseriesChannel<Signal> m_signal_channel;

    std::list<std::shared_ptr<ISubscription>> m_channel_subs;

    Ratchet m_ratchet;
};
