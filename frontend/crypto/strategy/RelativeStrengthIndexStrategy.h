#pragma once

#include "JsonStrategyConfig.h"
#include "RelativeStrengthIndex.h"
#include "StrategyInterface.h"

class RelativeStrengthIndexStrategyConfig
{
public:
    RelativeStrengthIndexStrategyConfig(const JsonStrategyConfig & json);

    static bool is_valid() { return true; }

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_timeframe = {};
    unsigned m_interval = {};
    unsigned m_margin = 0;
};

class RelativeStrengthIndexStrategy : public IStrategy
{
public:
    RelativeStrengthIndexStrategy(const RelativeStrengthIndexStrategyConfig &);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double>) override { return {}; }
    std::optional<Signal> push_candle(const Candle & c) override;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    RelativeStrengthIndexStrategyConfig m_config;
    RelativeStrengthIndex m_rsi;
    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
};
