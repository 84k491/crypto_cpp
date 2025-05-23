#pragma once

#include "EventLoopSubscriber.h"
#include "ScopeExit.h"
#include "StrategyBase.h"

class MockStrategy : public StrategyBase
{
public:
    MockStrategy(
            EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
            EventTimeseriesChannel<double> & price_channel)
    {
        m_channel_subs.push_back(price_channel.subscribe(
                event_loop.m_event_loop,
                [](auto) {},
                [this](const auto & ts, const double & price) {
                    if (const auto signal_opt = push_price({ts, price}); signal_opt) {
                        m_signal_channel.push(ts, signal_opt.value());
                    }
                }));
    }

    ~MockStrategy() override = default;

    std::optional<Signal> push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
    {
        ScopeExit se([&] { m_next_signal_side = std::nullopt; });
        if (m_next_signal_side.has_value()) {
            Signal result = {.side = *m_next_signal_side, .timestamp = ts_and_price.first, .price = ts_and_price.second};
            return result;
        }
        return std::nullopt;
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
    std::optional<Side> m_next_signal_side = Side::buy();
};
