#pragma once

#include "ExitStrategyBase.h"
#include "JsonStrategyConfig.h"

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
    TrailigStopLossStrategy(Symbol symbol,
                            JsonStrategyConfig config,
                            ITradingGateway & gateway);

    ~TrailigStopLossStrategy() override = default;

    [[nodiscard]] std::optional<std::string> on_price_changed(
            std::pair<std::chrono::milliseconds, double> ts_and_price) override;
    [[nodiscard]] std::optional<std::string> on_trade(
            const std::optional<OpenedPosition> & opened_position,
            const Trade & trade) override;

    [[nodiscard]] std::optional<std::pair<std::string, bool>>
    handle_event(const TpslResponseEvent & response) override;

    [[nodiscard]] std::optional<std::pair<std::string, bool>>
    handle_event(const TpslUpdatedEvent & response) override;

    [[nodiscard]] std::optional<std::pair<std::string, bool>>
    handle_event(const TrailingStopLossResponseEvent & response) override;

    [[nodiscard]] std::optional<std::pair<std::string, bool>>
    handle_event(const TrailingStopLossUpdatedEvent & response) override;

private:
    TrailingStopLoss calc_trailing_stop(const Trade & trade);
    void send_trailing_stop(TrailingStopLoss trailing_stop);
    StopLoss calc_current_stop() const;

private:
    Symbol m_symbol;

    std::set<xg::Guid> m_pending_requests;
    std::optional<OpenedPosition> m_opened_position;

    std::optional<TrailingStopLoss> m_active_stop_loss;

    TrailigStopLossStrategyConfig m_config;
};
