#pragma once

#include "Candle.h"
#include "JsonStrategyConfig.h"
#include "StrategyInterface.h"

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

class RateOfChangeStrategy : public IStrategy
{
    static constexpr size_t s_roc_interval = 1;

public:
    RateOfChangeStrategy(
            const RateOfChangeStrategyConfig & config,
            EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
            EventTimeseriesChannel<Candle> & candle_channel);

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;
    EventTimeseriesChannel<Signal> & signal_channel() override;

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    std::optional<Signal> push_candle(const Candle &);

private:
    std::list<double> m_prev_closing_prices;
    int m_trigger_iter = 0;

    RateOfChangeStrategyConfig m_config;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
    EventTimeseriesChannel<Signal> m_signal_channel;

    std::list<std::shared_ptr<ISubscription>> m_channel_subs;
};
