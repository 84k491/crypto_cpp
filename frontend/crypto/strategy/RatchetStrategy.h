#pragma once

#include "JsonStrategyConfig.h"
#include "Ratchet.h"
#include "StrategyInterface.h"

class RatchetStrategyConfig
{
public:
    RatchetStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    double m_retracement;
    std::chrono::milliseconds m_timeframe;
};

class RatchetStrategy : public IStrategy
{
public:
    RatchetStrategy(RatchetStrategyConfig config);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ) override { return {}; }
    std::optional<Signal> push_candle(const Candle &) override;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    RatchetStrategyConfig m_config;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;

    Ratchet m_ratchet;
};
