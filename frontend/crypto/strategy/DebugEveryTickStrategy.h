#pragma once

#include "DynamicTrailingStopLossStrategy.h"
#include "JsonStrategyConfig.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"

#include <chrono>
#include <optional>

class DebugEveryTickStrategyConfig
{
public:
    DebugEveryTickStrategyConfig(const JsonStrategyConfig & json);

    static bool is_valid();

    JsonStrategyConfig to_json() const;
    JsonStrategyConfig make_exit_strategy_config() const;

    // exit
    double m_risk;
    double m_no_loss_coef;
};

class DebugEveryTickStrategy final : public StrategyBase
{
    static constexpr unsigned max_iter = 10;

public:
    using ConfigT = DebugEveryTickStrategyConfig;

    DebugEveryTickStrategy(
            const DebugEveryTickStrategyConfig & conf,
            std::shared_ptr<EventLoop> & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;
    std::optional<std::chrono::milliseconds> timeframe() const override { return {}; }

private:
    void push_price(std::pair<std::chrono::milliseconds, double> ts_and_price);

private:
    const DebugEveryTickStrategyConfig m_config;

    DynamicTrailingStopLossStrategy m_exit_strategy;

    Side last_side = Side::sell();

    unsigned iteration = 0;

    EventSubcriber m_sub;
};
