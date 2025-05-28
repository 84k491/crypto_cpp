#pragma once

#include "BollingerBands.h"
#include "JsonStrategyConfig.h"
#include "Signal.h"
#include "StrategyBase.h"

#include <chrono>

class BollingerBandsStrategyConfig
{
public:
    BollingerBandsStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_interval = {};
    double m_std_deviation_coefficient = 0.;
};

class BollingerBandsStrategy : public StrategyBase
{
public:
    using ConfigT = BollingerBandsStrategyConfig;
    BollingerBandsStrategy(
            const BollingerBandsStrategyConfig & config,
            EventLoopSubscriber & event_loop,
            EventTimeseriesChannel<double> & price_channel);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price);

private:
    BollingerBandsStrategyConfig m_config;

    BollingerBands m_bollinger_bands;

    std::optional<Side> m_last_signal_side;
};
