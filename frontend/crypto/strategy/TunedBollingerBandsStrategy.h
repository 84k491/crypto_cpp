#pragma once

#include "BollingerBands.h"
#include "JsonStrategyConfig.h"
#include "Signal.h"
#include "StrategyInterface.h"

#include <chrono>

class TunedBollingerBandsStrategyConfig
{
public:
    TunedBollingerBandsStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_interval = {};
    double m_std_deviation_coefficient = 0.;
};

class TunedBollingerBandsStrategy : public IStrategy
{
    static constexpr std::chrono::milliseconds price_filter_interval = std::chrono::minutes(4);

public:
    using ConfigT = TunedBollingerBandsStrategyConfig;
    TunedBollingerBandsStrategy(const TunedBollingerBandsStrategyConfig & config);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) override;

    EventTimeseriesPublisher<std::tuple<std::string, std::string, double>> & strategy_internal_data_publisher() override;
    bool is_valid() const override;

private:
    TunedBollingerBandsStrategyConfig m_config;

    MovingAverage m_price_filter;
    std::optional<double> m_last_filtered_price_opt;

    BollingerBands m_bollinger_bands;

    std::optional<Side> m_last_signal_side;

    EventTimeseriesPublisher<std::tuple<std::string, std::string, double>> m_strategy_internal_data_publisher;
};
