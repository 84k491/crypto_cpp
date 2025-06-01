#pragma once

#include "ExitStrategyBase.h"
#include "JsonStrategyConfig.h"
#include "StrategyChannels.h"

#include <set>

class TrailigStopLossStrategyConfig
{
public:
    TrailigStopLossStrategyConfig(const JsonStrategyConfig & config);
    TrailigStopLossStrategyConfig(double risk);

    auto risk() const { return m_risk; }

private:
    double m_risk;
};

class TrailigStopLossStrategy : public ExitStrategyBase
{
public:
    TrailigStopLossStrategy(
            Symbol symbol,
            JsonStrategyConfig config,
            EventLoopSubscriber & event_loop,
            ITradingGateway & gateway,
            StrategyChannelsRefs channels);

protected:
    void handle_event(const TrailingStopLossResponseEvent & response);
    void handle_event(const TrailingStopLossUpdatedEvent & response);

    virtual void on_price_changed(
            std::pair<std::chrono::milliseconds, double> /* ts_and_price */) {}

    void on_trade(const Trade & trade);

    TrailingStopLoss calc_trailing_stop(const Trade & trade);
    void send_trailing_stop(TrailingStopLoss trailing_stop);

    void on_error(const std::string &, bool);

protected:
    Symbol m_symbol;

    std::set<xg::Guid> m_pending_requests;

    bool m_is_pos_opened = false;

    std::optional<TrailingStopLoss> m_active_stop_loss;

    TrailigStopLossStrategyConfig m_config;
};
