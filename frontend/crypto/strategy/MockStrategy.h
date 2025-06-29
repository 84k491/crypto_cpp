#pragma once

#include "EventLoopSubscriber.h"
#include "ScopeExit.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"

class MockStrategy : public StrategyBase
{
public:
    MockStrategy(
            EventLoopSubscriber & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders)
        : StrategyBase(orders)
    {
        m_channel_subs.push_back(channels.price_channel.subscribe(
                event_loop.m_event_loop,
                [](const auto &) {},
                [this](const auto & ts, const double & price) {
                    push_price({ts, price});
                }));
    }

    ~MockStrategy() override = default;

    void push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
    {
        ScopeExit se([&] { m_next_signal_side = std::nullopt; });

        if (m_next_signal_side.has_value()) {
            try_send_order(*m_next_signal_side, ts_and_price.second, ts_and_price.first);
        }
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
