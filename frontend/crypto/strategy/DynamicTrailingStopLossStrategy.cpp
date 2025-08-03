#include "DynamicTrailingStopLossStrategy.h"

#include "Logger.h"
#include "OrderManager.h"

DynamicTrailigStopLossStrategyConfig::DynamicTrailigStopLossStrategyConfig(const JsonStrategyConfig & config)
{
    if (config.get().contains("risk")) {
        m_risk = config.get()["risk"].get<double>();
    }
    if (config.get().contains("no_loss_coef")) {
        m_no_loss_coef = config.get()["no_loss_coef"].get<double>();
    }
}

DynamicTrailigStopLossStrategyConfig::DynamicTrailigStopLossStrategyConfig(double risk, double no_loss_coef)
    : m_risk(risk)
    , m_no_loss_coef(no_loss_coef)
{
}

JsonStrategyConfig DynamicTrailigStopLossStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

DynamicTrailingStopLossStrategy::DynamicTrailingStopLossStrategy(
        OrderManager & orders,
        JsonStrategyConfig config,
        EventLoopSubscriber & event_loop,
        ITradingGateway & gateway,
        StrategyChannelsRefs channels)
    : TrailigStopLossStrategy(
              orders,
              config,
              event_loop,
              gateway,
              channels)
    , m_dynamic_config(config)
{
    m_channel_subs.push_back(channels.price_levels_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto &, const auto & price_levels) {
                m_last_pos_price_levels = price_levels;
            }));
}

void DynamicTrailingStopLossStrategy::on_price_changed(
        std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    if (!m_tsl_sub || !m_active_stop_loss) {
        m_triggered_once = false;
        return;
    }

    if (m_triggered_once) {
        return;
    }

    const auto & [ts, price] = ts_and_price;

    const int side_sign = m_active_stop_loss->side().opposite().sign();

    // TODO doesn't count fees!
    const auto desired_price_distance = m_active_stop_loss->price_distance() * m_dynamic_config.no_loss_coef();
    const auto no_risk_trigger_price = m_last_pos_price_levels.no_loss_price + (desired_price_distance * side_sign);

    const auto price_distance_side_abs = [&](double current_price, double ref) {
        // positive if price is less than level for short
        const double pd_abs = (current_price - ref) * side_sign;
        return pd_abs;
    };

    if (price_distance_side_abs(price, no_risk_trigger_price) < 0.) {
        return;
    }

    const auto new_trailing_stop = TrailingStopLoss{
            m_active_stop_loss->symbol_name(),
            desired_price_distance,
            m_active_stop_loss->side()};

    Logger::logf<LogLevel::Status>("Updating stop loss' price distance to {}", desired_price_distance);

    m_tsl_sub = m_orders.send_trailing_stop(
                                new_trailing_stop,
                                ts)
                        .subscribe(
                                m_event_loop.m_event_loop,
                                [this](const auto & tsl) {
                                    on_trailing_stop_updated(tsl);
                                });

    m_triggered_once = true;
}
