#pragma once

#include "EventLoopSubscriber.h"
#include "ScopeExit.h"
#include "StrategyBase.h"
#include "StrategyChannels.h"
#include "TpslExitStrategy.h"

class MockStrategy : public StrategyBase
{
public:
    MockStrategy(
            const JsonStrategyConfig &,
            EventLoop & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders)
        : StrategyBase(orders, event_loop, channels)
        , m_exit_strategy(
                  orders,
                  JsonStrategyConfig{R"({"risk":0.1,"risk_reward_ratio":0.8")"},
                  event_loop,
                  channels)
        , m_sub{event_loop}
    {
        m_sub.subscribe(
                channels.price_channel,
                [](const auto &) {},
                [this](const auto & ts, const double & price) {
                    push_price({ts, price});
                });

        m_sub.subscribe(
                m_exit_strategy.error_channel(),
                [&](const std::pair<std::string, bool> & err) {
                    m_error_channel.push(err);
                });
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

    TpslExitStrategy m_exit_strategy;

    EventSubcriber m_sub;
};
