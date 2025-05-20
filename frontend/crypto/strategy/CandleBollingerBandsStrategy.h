#pragma once

#include "BollingerBands.h"
#include "Candle.h"
#include "JsonStrategyConfig.h"
#include "Signal.h"
#include "StrategyInterface.h"

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

class CandleBollingerBandsStrategy : public IStrategy
{
    static constexpr std::chrono::milliseconds price_filter_interval = std::chrono::minutes(4);

public:
    using ConfigT = CandleBollingerBandsStrategyConfig;
    CandleBollingerBandsStrategy(
            const CandleBollingerBandsStrategyConfig & config,
            EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
            EventTimeseriesChannel<Candle> & candle_channel);

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;
    EventTimeseriesChannel<Signal> & signal_channel() override;

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    std::optional<Signal> push_candle(const Candle &);

private:
    CandleBollingerBandsStrategyConfig m_config;

    BollingerBands m_bollinger_bands;

    std::optional<Side> m_last_signal_side;

    int m_candles_above_price_trigger = 0;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
    EventTimeseriesChannel<Signal> m_signal_channel;

    std::list<std::shared_ptr<ISubscription>> m_channel_subs;
};
