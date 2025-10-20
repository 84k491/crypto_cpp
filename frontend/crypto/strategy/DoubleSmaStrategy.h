#pragma once

#include "DynamicTrailingStopLossStrategy.h"
#include "JsonStrategyConfig.h"
#include "SimpleMovingAverage.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"

#include <chrono>
#include <optional>

class DoubleSmaStrategyConfig
{
public:
    DoubleSmaStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;
    JsonStrategyConfig make_exit_strategy_config() const;

    std::chrono::milliseconds m_slow_interval = {};
    std::chrono::milliseconds m_fast_interval = {};

    // exit
    double m_risk;
    double m_no_loss_coef;
};

class DoubleSmaStrategy final : public StrategyBase
{
public:
    using ConfigT = DoubleSmaStrategyConfig;

    DoubleSmaStrategy(
            const DoubleSmaStrategyConfig & conf,
            EventLoop & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override { return {}; }

private:
    void push_price(std::pair<std::chrono::milliseconds, double> ts_and_price);

private:
    const DoubleSmaStrategyConfig m_config;

    DynamicTrailingStopLossStrategy m_exit_strategy;

    SimpleMovingAverage m_slow_avg;
    SimpleMovingAverage m_fast_avg;

    std::optional<double> m_prev_slow_avg;
    std::optional<double> m_prev_fast_avg;

    EventSubcriber m_sub;
};
