#include "TrailingStopStrategy.h"

#include "EventLoop.h"
#include "Logger.h"
#include "OrderManager.h"


TrailigStopLossStrategyConfig::TrailigStopLossStrategyConfig(const JsonStrategyConfig & config)
{
    if (config.get().contains("risk")) {
        m_risk = config.get()["risk"].get<double>();
    }
    LOG_DEBUG("TrailigStopLossStrategyConfig c-tor");
}

TrailigStopLossStrategyConfig::TrailigStopLossStrategyConfig(double risk)
    : m_risk(risk)
{
}

TrailigStopLossStrategy::TrailigStopLossStrategy(
        OrderManager & orders,
        JsonStrategyConfig config,
        EventLoop & event_loop,
        StrategyChannelsRefs channels)
    : m_orders(orders)
    , m_event_loop(event_loop)
    , m_channels(channels)
    , m_config(config)
    , m_main_sub(event_loop)
{
    m_main_sub.subscribe(
            channels.price_channel,
            [](const auto &) {},
            [this](const auto & ts, const double & price) {
                on_price_changed({ts, price});
            });
    m_main_sub.subscribe(
            channels.opened_pos_channel,
            [this](const bool & v) {
                m_is_pos_opened = v;
            });

    m_main_sub.subscribe(
            channels.trades_channel,
            [](const auto &) {},
            [this](const auto &, const auto & trade) {
                on_trade(trade);
            });
}

void TrailigStopLossStrategy::on_trade(const Trade & trade)
{
    if (m_is_pos_opened && m_tsl_sub) {
        // position opened
        // stop loss is set already
        const std::string_view msg = "TrailigStopLossStrategy: active stop loss already exists or pending";
        LOG_WARNING(std::string(msg));
        return;
    }

    // TODO handle a double trade case, add test
    if (!m_is_pos_opened) {
        return;
    }

    const auto tsl = calc_trailing_stop(trade);
    auto & ch = m_orders.send_trailing_stop(
            tsl,
            trade.ts());

    m_tsl_sub = std::make_unique<EventSubcriber>(m_event_loop);
    m_tsl_sub->subscribe(
            ch,
            [this](const auto & tsl) {
                on_trailing_stop_updated(tsl);
            });
}

TrailingStopLoss TrailigStopLossStrategy::calc_trailing_stop(const Trade & trade)
{
    const double & entry_price = trade.price();
    const double total_fee = trade.fee() * 2;
    const double fee_price_delta = total_fee / trade.unsigned_volume().value();

    const double risk = entry_price * m_config.risk();
    const double price_delta = risk - fee_price_delta;

    const auto opposite_side = trade.side().opposite();
    const auto stop_loss = TrailingStopLoss(trade.symbol_name(), price_delta, opposite_side);
    return stop_loss;
}

void TrailigStopLossStrategy::on_trailing_stop_updated(const std::shared_ptr<TrailingStopLoss> & tsl)
{
    m_active_stop_loss = tsl;

    if (!tsl || !tsl->m_active_stop_loss_price.has_value()) {
        m_tsl_sub.reset();
        m_active_stop_loss.reset();
        LOG_DEBUG("Trailing stop loss removed");
        return;
    }

    StopLoss sl{
            tsl->symbol_name(),
            tsl->m_active_stop_loss_price.value_or(0),
            tsl->side()};

    m_trailing_stop_channel.push(tsl->m_update_ts, sl);
    m_channels.trailing_stop_loss_channel.push(tsl->m_update_ts, sl);
}
