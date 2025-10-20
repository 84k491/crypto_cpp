#pragma once

#include "BollingerBands.h"
#include "DynamicTrailingStopLossStrategy.h"
#include "JsonStrategyConfig.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"

#include <chrono>

class BollingerBandsStrategyConfig
{
public:
    BollingerBandsStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;
    JsonStrategyConfig make_exit_strategy_config() const;

    std::chrono::milliseconds m_interval = {};
    double m_std_deviation_coefficient = 0.;

    // exit
    double m_risk;
    double m_no_loss_coef;
};

class BollingerBandsStrategy : public StrategyBase
{
public:
    using ConfigT = BollingerBandsStrategyConfig;
    BollingerBandsStrategy(
            const BollingerBandsStrategyConfig & config,
            EventLoop & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    void push_price(std::pair<std::chrono::milliseconds, double> ts_and_price);

private:
    BollingerBandsStrategyConfig m_config;

    DynamicTrailingStopLossStrategy m_exit_strategy;

    BollingerBands m_bollinger_bands;

    std::optional<Side> m_last_signal_side;

    EventSubcriber m_sub;
};
