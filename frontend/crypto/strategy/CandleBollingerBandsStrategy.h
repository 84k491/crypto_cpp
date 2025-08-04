#pragma once

#include "BollingerBands.h"
#include "Candle.h"
#include "DynamicTrailingStopLossStrategy.h"
#include "JsonStrategyConfig.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"

#include <chrono>

class CandleBollingerBandsStrategyConfig
{
public:
    CandleBollingerBandsStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;
    JsonStrategyConfig make_exit_strategy_config() const;

    std::chrono::milliseconds m_timeframe = {};
    std::chrono::milliseconds m_interval = {};
    int m_candles_threshold = 0;
    double m_std_deviation_coefficient = 0.;

    // exit
    double m_risk;
    double m_no_loss_coef;
};

class CandleBollingerBandsStrategy : public StrategyBase
{
public:
    using ConfigT = CandleBollingerBandsStrategyConfig;
    CandleBollingerBandsStrategy(
            const CandleBollingerBandsStrategyConfig & config,
            EventLoopSubscriber & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    void push_candle(const Candle &);

private:
    CandleBollingerBandsStrategyConfig m_config;

    DynamicTrailingStopLossStrategy m_exit_strategy;

    BollingerBands m_bollinger_bands;

    std::optional<Side> m_last_signal_side;

    int m_candles_above_price_trigger = 0;
};
