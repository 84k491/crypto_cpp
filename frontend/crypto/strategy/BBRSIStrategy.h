#pragma once

#include "BollingerBands.h"
#include "JsonStrategyConfig.h"
#include "RelativeStrengthIndex.h"
#include "StrategyInterface.h"

#include <chrono>

struct BBRSIStrategyConfig
{
    BBRSIStrategyConfig(const JsonStrategyConfig & json);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_timeframe = {};
    double m_std_deviation_coefficient = 0.;
    unsigned m_bb_interval = {};
    unsigned m_rsi_interval = {};
    unsigned m_margin = 0;
};

class BBRSIStrategy : public IStrategy
{
public:
    BBRSIStrategy(BBRSIStrategyConfig config);

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double>) override { return {}; }
    std::optional<Signal> push_candle(const Candle & c) override;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override;

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    BBRSIStrategyConfig m_config;

    BollingerBands m_bollinger_bands;
    std::optional<Side> m_last_signal_side;
    bool m_price_in_trigger_zone_bb = false;

    unsigned m_rsi_top_threshold = 100;
    RelativeStrengthIndex m_rsi;
    unsigned m_rsi_bot_threshold = 0;

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;
};
