#pragma once

#include "TrailingStopStrategy.h"

class DynamicTrailigStopLossStrategyConfig
{
public:
    DynamicTrailigStopLossStrategyConfig(const JsonStrategyConfig & config);
    DynamicTrailigStopLossStrategyConfig(double risk, double no_loss_coef);

    auto risk() const { return m_risk; }
    auto no_loss_coef() const { return m_no_loss_coef; }

    JsonStrategyConfig to_json() const;

private:
    double m_risk;
    double m_no_loss_coef;
};

class DynamicTrailingStopLossStrategy : public TrailigStopLossStrategy
{
public:
    DynamicTrailingStopLossStrategy(
            OrderManager & orders,
            JsonStrategyConfig config,
            EventLoop & event_loop,
            StrategyChannelsRefs channels);

private:
    void on_price_changed(
            std::pair<std::chrono::milliseconds, double> ts_and_price) override;

private:
    ProfitPriceLevels m_last_pos_price_levels = {};
    DynamicTrailigStopLossStrategyConfig m_dynamic_config; // TODO unify it with m_config

    bool m_triggered_once = false;

    EventLoop & m_event_loop;
    EventSubcriber m_sub;
};
