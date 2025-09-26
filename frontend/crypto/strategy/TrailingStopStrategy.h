#pragma once

#include "EventLoopSubscriber.h"
#include "ExitStrategyBase.h"
#include "JsonStrategyConfig.h"
#include "StrategyChannels.h"

class TrailigStopLossStrategyConfig
{
public:
    TrailigStopLossStrategyConfig(const JsonStrategyConfig & config);
    TrailigStopLossStrategyConfig(double risk);

    auto risk() const { return m_risk; }

private:
    double m_risk;
};

class OrderManager;
class TrailigStopLossStrategy : public ExitStrategyBase
{
public:
    TrailigStopLossStrategy(
            OrderManager & orders,
            JsonStrategyConfig config,
            std::shared_ptr<EventLoop> & event_loop,
            StrategyChannelsRefs channels);

protected:
    virtual void on_price_changed(
            std::pair<std::chrono::milliseconds, double> /* ts_and_price */) {}

    void on_trade(const Trade & trade);

    TrailingStopLoss calc_trailing_stop(const Trade & trade);

    void on_trailing_stop_updated(const std::shared_ptr<TrailingStopLoss> & tsl);

protected:
    OrderManager & m_orders;
    std::shared_ptr<EventLoop> m_event_loop;
    StrategyChannelsRefs m_channels;
    TrailigStopLossStrategyConfig m_config;

    bool m_is_pos_opened = false;

    std::shared_ptr<TrailingStopLoss> m_active_stop_loss;
    EventSubcriber m_main_sub;
    std::optional<EventSubcriber> m_tsl_sub;
};
