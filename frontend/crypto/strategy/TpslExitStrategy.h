#pragma once

#include "ConditionalOrders.h"
#include "EventLoop.h"
#include "EventLoopSubscriber.h"
#include "ExitStrategyBase.h"
#include "ISubsription.h"
#include "JsonStrategyConfig.h"
#include "Position.h"
#include "StrategyChannels.h"
#include "Tpsl.h"

#include <set>

class Trade;

class TpslExitStrategyConfig
{
    friend std::ostream & operator<<(std::ostream & os, const TpslExitStrategyConfig & config);

public:
    TpslExitStrategyConfig(const JsonStrategyConfig & json);
    TpslExitStrategyConfig(double risk, double risk_reward_ratio);

    JsonStrategyConfig to_json() const;

    bool is_valid() const;

    auto risk() const { return m_risk; }
    auto risk_reward_ratio() const { return m_risk_reward_ratio; }

private:
    double m_risk = 1.;
    double m_risk_reward_ratio = 1.;
};

class OrderManager;
class TpslExitStrategy : public ExitStrategyBase
{
public:
    TpslExitStrategy(
            OrderManager & orders,
            JsonStrategyConfig config,
            EventLoopSubscriber & event_loop,
            StrategyChannelsRefs channels);

private:
    void on_trade(const Trade & trade);

    Tpsl calc_tpsl(const Trade & trade);
    void on_error(std::string, bool);

private:
    OrderManager & m_orders;
    TpslExitStrategyConfig m_config;
    EventLoopSubscriber m_event_loop;

    std::pair<std::chrono::milliseconds, double> m_last_ts_and_price;
    bool m_is_pos_opened = false;

    std::optional<OpenedPosition> m_opened_position;

    std::shared_ptr<ISubscription> m_sub;
    std::shared_ptr<TpslFullPos> m_active_tpsl;
};
