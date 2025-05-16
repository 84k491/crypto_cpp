#pragma once

#include "ScopeExit.h"
#include "StrategyInterface.h"

class MockStrategy : public IStrategy
{
public:
    ~MockStrategy() override = default;

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price) override
    {
        ScopeExit se([&] { m_next_signal_side = std::nullopt; });
        if (m_next_signal_side.has_value()) {
            Signal result = {.side = *m_next_signal_side, .timestamp = ts_and_price.first, .price = ts_and_price.second};
            return result;
        }
        return std::nullopt;
    }
    std::optional<Signal> push_candle(const Candle &) override { return {}; }

    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & strategy_internal_data_channel() override
    {
        return m_strategy_internal_data_channel;
    }

    bool is_valid() const override { return true; }

    std::optional<std::chrono::milliseconds> timeframe() const override
    {
        return {};
    }

public:
    void signal_on_next_tick(const Side & signal_side)
    {
        m_next_signal_side = signal_side;
    }

private:
    EventTimeseriesChannel<std::tuple<std::string, std::string, double>> m_strategy_internal_data_channel;

    std::optional<Side> m_next_signal_side = Side::buy();
};

