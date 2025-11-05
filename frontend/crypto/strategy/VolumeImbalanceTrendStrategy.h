#pragma once

#include "DynamicTrailingStopLossStrategy.h"
#include "JsonStrategyConfig.h"
#include "StandardDeviation.h"
#include "StrategyBase.h"
#include "TimeWeightedMovingAverage.h"

struct VolumeImbalanceTrendStrategyConfig
{
    VolumeImbalanceTrendStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;
    JsonStrategyConfig make_exit_strategy_config() const;

    std::chrono::milliseconds m_timeframe = {};

    unsigned m_min_volume_imbalance_perc;
    unsigned m_min_price_change_perc;
    unsigned m_min_trade_rate_threshold_perc = {};
    double m_max_relative_volatility_perc;
    std::chrono::milliseconds m_lookback_period = {};

    // exit
    double m_risk;
    double m_no_loss_coef;
};

/*
Signals when:
    imbalance > 0.4 (strong side dominance)
    price_return > 0.05%
    trade_rate > threshold (above rolling median?)
    volatility within normal range (no spikes)

    TODO: verify signal with some other indicator.
          eg RSI: it must be not in overbought zone for long
*/
class VolumeImbalanceTrendStrategy : public StrategyBase
{
public:
    VolumeImbalanceTrendStrategy(
            const VolumeImbalanceTrendStrategyConfig & config,
            EventLoop & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    void push_candle(const Candle &);

private:
    VolumeImbalanceTrendStrategyConfig m_config;

    DynamicTrailingStopLossStrategy m_exit_strategy;

    TimeWeightedMovingAverage m_trade_rate_avg;
    std::optional<double> m_prev_trade_rate_avg_opt;
    StandardDeviation m_std;

    EventSubcriber m_sub;
};
