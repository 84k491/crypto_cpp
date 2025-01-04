#pragma once

#include "BollingerBands.h"
#include "JsonStrategyConfig.h"
#include "Signal.h"
#include "StrategyInterface.h"

#include <chrono>

class BollingerBandsStrategyConfig
{
public:
    BollingerBandsStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_interval = {};
    double m_std_deviation_coefficient = 0.;
};

class BollingerBandsStrategy : public IStrategy
{
public:
    using ConfigT = BollingerBandsStrategyConfig;
    BollingerBandsStrategy(const BollingerBandsStrategyConfig & config);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) override;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;

    bool is_valid() const override;

private:
    BollingerBandsStrategyConfig m_config;

    BollingerBands m_bollinger_bands;

    std::optional<Side> m_last_signal_side;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
};
