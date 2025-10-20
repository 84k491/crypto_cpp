#pragma once

#include "BollingerBands.h"
#include "DynamicTrailingStopLossStrategy.h"
#include "JsonStrategyConfig.h"
#include "RelativeStrengthIndex.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"

#include <chrono>

struct BBRSIStrategyConfig
{
    BBRSIStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;
    JsonStrategyConfig make_exit_strategy_config() const;

    std::chrono::milliseconds m_timeframe = {};
    double m_std_deviation_coefficient = 0.;
    unsigned m_bb_interval = {};
    unsigned m_rsi_interval = {};
    unsigned m_margin = 0;

    // exit
    double m_risk;
    double m_no_loss_coef;
};

class BBRSIStrategy : public StrategyBase
{
public:
    BBRSIStrategy(
            BBRSIStrategyConfig config,
            EventLoop & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    void push_candle(const Candle & c);

private:
    BBRSIStrategyConfig m_config;
    DynamicTrailingStopLossStrategy m_exit_strategy;

    BollingerBands m_bollinger_bands;
    std::optional<Side> m_last_signal_side;
    bool m_price_in_trigger_zone_bb = false;

    unsigned m_rsi_top_threshold = 100;
    RelativeStrengthIndex m_rsi;
    unsigned m_rsi_bot_threshold = 0;

    EventSubcriber m_sub;
};
