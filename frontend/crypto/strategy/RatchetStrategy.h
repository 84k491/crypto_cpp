#pragma once

#include "Candle.h"
#include "JsonStrategyConfig.h"
#include "Ratchet.h"
#include "StrategyBase.h"

class RatchetStrategyConfig
{
public:
    RatchetStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    double m_retracement;
    std::chrono::milliseconds m_timeframe;
};

class RatchetStrategy : public StrategyBase
{
public:
    RatchetStrategy(
            RatchetStrategyConfig config,
            EventLoopSubscriber & event_loop,
            EventTimeseriesChannel<Candle> & candle_channel);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    std::optional<Signal> push_candle(const Candle &);

private:
    RatchetStrategyConfig m_config;

    Ratchet m_ratchet;
};
