#pragma once

#include "DynamicTrailingStopLossStrategy.h"
#include "JsonStrategyConfig.h"
#include "RelativeStrengthIndex.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"

class RelativeStrengthIndexStrategyConfig
{
public:
    RelativeStrengthIndexStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;
    JsonStrategyConfig make_exit_strategy_config() const;

    std::chrono::milliseconds m_timeframe = {};
    unsigned m_interval = {};
    unsigned m_margin = 0;

    // exit
    double m_risk;
    double m_no_loss_coef;
};

class RelativeStrengthIndexStrategy : public StrategyBase
{
public:
    RelativeStrengthIndexStrategy(
            const RelativeStrengthIndexStrategyConfig & config,
            std::shared_ptr<EventLoop> & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    void push_candle(const Candle & c);

private:
    RelativeStrengthIndexStrategyConfig m_config;
    DynamicTrailingStopLossStrategy m_exit_strategy;

    RelativeStrengthIndex m_rsi;

    EventSubcriber m_sub;
};
