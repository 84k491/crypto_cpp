#pragma once

#include "AverageDirectionalIndex.h"
#include "EventLoopSubscriber.h"
#include "DynamicGridWithBan.h"
#include "JsonStrategyConfig.h"
#include "OrderManager.h"
#include "StrategyChannels.h"

struct DynamicGridAdxStrategyConfig
{
    DynamicGridAdxStrategyConfig(JsonStrategyConfig);

    bool is_valid() const;

    JsonStrategyConfig to_json() const;

    std::chrono::milliseconds m_timeframe = {};
    size_t m_interval = 0;
    unsigned m_levels_per_side = 0;
    double m_price_radius_perc = 0.; // to the end of the last level

    size_t m_adx_interval = 0;
    double m_adx_threshold = 0;
};

class DynamicGridAdxStrategy : public DynamicGridWithBan
{
public:
    DynamicGridAdxStrategy(
            const DynamicGridAdxStrategyConfig & config,
            EventLoopSubscriber & event_loop,
            StrategyChannelsRefs channels,
            OrderManager & orders);

    bool is_valid() const override;

    std::optional<std::chrono::milliseconds> timeframe() const override;

private:
    bool is_banned(std::chrono::milliseconds ts, const Candle & candle) override;

private:
    // EventLoopSubscriber & m_event_loop;

    DynamicGridAdxStrategyConfig m_config;
    AverageDirectionalIndex m_adx;

    std::chrono::milliseconds m_ban_until = {};
};
