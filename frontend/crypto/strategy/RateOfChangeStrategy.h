#pragma once

#include "Candle.h"
#include "JsonStrategyConfig.h"
#include "StrategyBase.h"

class RateOfChangeStrategyConfig
{
public:
    RateOfChangeStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_timeframe = {};
    int m_trigger_interval = {};
    double m_signal_threshold = 0.;
};

class RateOfChangeStrategy : public StrategyBase
{
    static constexpr size_t s_roc_interval = 1;

public:
    RateOfChangeStrategy(
            const RateOfChangeStrategyConfig & config,
            EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
            EventTimeseriesChannel<Candle> & candle_channel);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    std::optional<Signal> push_candle(const Candle &);

private:
    std::list<double> m_prev_closing_prices;
    int m_trigger_iter = 0;

    RateOfChangeStrategyConfig m_config;
};
