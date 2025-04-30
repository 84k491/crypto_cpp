#pragma once

#include "EventLoop.h"
#include "ExitStrategyBase.h"
#include "JsonStrategyConfig.h"
#include "Position.h"
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

class TpslExitStrategy : public ExitStrategyBase
{
public:
    TpslExitStrategy(
            Symbol symbol,
            JsonStrategyConfig config,
            EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
            ITradingGateway & gateway,
            EventTimeseriesChannel<double> & price_channel,
            EventObjectChannel<bool> & opened_pos_channel,
            EventTimeseriesChannel<Trade> & trades_channel);

private:
    void on_trade(const Trade & trade);

    void handle_event(const TpslResponseEvent & response);
    void handle_event(const TpslUpdatedEvent & response);

    Tpsl calc_tpsl(const Trade & trade);
    void send_tpsl(Tpsl tpsl);

    void on_error(std::string, bool);

private:
    TpslExitStrategyConfig m_config;

    std::pair<std::chrono::milliseconds, double> m_last_ts_and_price;
    bool m_is_pos_opened = false;

    Symbol m_symbol;
    std::set<xg::Guid> m_pending_requests;
    std::optional<OpenedPosition> m_opened_position;
    std::optional<Tpsl> m_active_tpsl;
};
