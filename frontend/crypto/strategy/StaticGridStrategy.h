#pragma once

#include "EventLoopSubscriber.h"
#include "OrderManager.h"
#include "StaticGridWithBan.h"
#include "StrategyChannels.h"

struct StaticGridStrategyConfig
{
    StaticGridStrategyConfig(JsonStrategyConfig);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_timeframe = {};
    size_t m_interval = 0;
    double m_level_width_perc = 0;
};

class StaticGridStrategy : public StaticGridWithBan
{
public:
    StaticGridStrategy(
            const StaticGridStrategyConfig & config,
            std::shared_ptr<EventLoop> & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_banned(std::chrono::milliseconds, const Candle &) override;
};
