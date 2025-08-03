#pragma once

#include "BollingerBands.h"
#include "Candle.h"
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

    std::chrono::milliseconds m_timeframe = {};
    std::chrono::milliseconds m_interval = {};
    int m_candles_threshold = 0;
    double m_std_deviation_coefficient = 0.;
};

class CandleBollingerBandsStrategy : public StrategyBase
{
    static constexpr std::chrono::milliseconds price_filter_interval = std::chrono::minutes(4);

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

    BollingerBands m_bollinger_bands;

    std::optional<Side> m_last_signal_side;

    int m_candles_above_price_trigger = 0;
};
