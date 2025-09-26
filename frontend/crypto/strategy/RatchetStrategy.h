#pragma once

#include "Candle.h"
#include "DynamicTrailingStopLossStrategy.h"
#include "JsonStrategyConfig.h"
#include "Ratchet.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"

class RatchetStrategyConfig
{
public:
    RatchetStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;
    JsonStrategyConfig make_exit_strategy_config() const;

    double m_retracement;
    std::chrono::milliseconds m_timeframe;

    // exit
    double m_risk;
    double m_no_loss_coef;
};

class RatchetStrategy : public StrategyBase
{
public:
    RatchetStrategy(
            RatchetStrategyConfig config,
            std::shared_ptr<EventLoop> & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    void push_candle(const Candle &);

private:
    RatchetStrategyConfig m_config;

    DynamicTrailingStopLossStrategy m_exit_strategy;

    Ratchet m_ratchet;

    EventSubcriber m_sub;
};
