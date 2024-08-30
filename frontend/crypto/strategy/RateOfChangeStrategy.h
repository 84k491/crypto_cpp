#pragma once

#include "JsonStrategyConfig.h"
#include "RateOfChange.h"
#include "StrategyInterface.h"

class RateOfChangeStrategyConfig
{
public:
    RateOfChangeStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_interval = {};
    double m_signal_threshold = 0.;
};

class RateOfChangeStrategy : public IStrategy
{
public:
    RateOfChangeStrategy(const RateOfChangeStrategyConfig & config);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) override;
    EventTimeseriesPublisher<std::tuple<std::string, std::string, double>> & strategy_internal_data_publisher() override;

    bool is_valid() const override;

private:
    RateOfChangeStrategyConfig m_config;
    RateOfChange m_rate_of_change;
    EventTimeseriesPublisher<std::tuple<std::string, std::string, double>> m_strategy_internal_data_publisher;
};
