#pragma once

#include "BollingerBands.h"
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
    CandleBollingerBandsStrategy(const CandleBollingerBandsStrategyConfig & config);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double>) override { return {}; };
    std::optional<Signal> push_candle(const Candle &) override;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;
    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    CandleBollingerBandsStrategyConfig m_config;

    BollingerBands m_bollinger_bands;

    std::optional<Side> m_last_signal_side;

    int m_candles_above_price_trigger = 0;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
};
