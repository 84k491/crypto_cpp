#include "TrailingStopStrategy.h"

#include "EventLoop.h"
#include "Logger.h"
#include "OrderManager.h"

#include <utility>

TrailigStopLossStrategyConfig::TrailigStopLossStrategyConfig(const JsonStrategyConfig & config)
{
    if (config.get().contains("risk")) {
        m_risk = config.get()["risk"].get<double>();
    }
    Logger::log<LogLevel::Debug>("TrailigStopLossStrategyConfig c-tor");
}

TrailigStopLossStrategyConfig::TrailigStopLossStrategyConfig(double risk)
    : m_risk(risk)
{
}

TrailigStopLossStrategy::TrailigStopLossStrategy(
        OrderManager & orders,
        JsonStrategyConfig config,
        EventLoopSubscriber & event_loop,
        ITradingGateway & gateway,
        StrategyChannelsRefs channels)

    : ExitStrategyBase(gateway)
    , m_orders(orders)
    , m_event_loop(event_loop)
    , m_config(config)
{
    m_channel_subs.push_back(channels.price_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto & ts, const double & price) {
                on_price_changed({ts, price});
            }));

    m_channel_subs.push_back(channels.opened_pos_channel.subscribe(
            event_loop.m_event_loop,
            [this](const bool & v) { m_is_pos_opened = v; }));

    m_channel_subs.push_back(channels.trades_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto &, const auto & trade) {
                on_trade(trade);
            }));
}

void TrailigStopLossStrategy::on_trade(const Trade & trade)
{
    if (m_is_pos_opened && m_tsl_sub != nullptr) {
        // position opened
        // stop loss is set already
        const std::string_view msg = "TrailigStopLossStrategy: active stop loss already exists or pending";
        Logger::log<LogLevel::Warning>(std::string(msg));
        return;
    }

    // TODO handle a double trade case, add test
    if (!m_is_pos_opened) {
        return;
    }

    const auto tsl = calc_trailing_stop(trade);
    m_tsl_sub = m_orders.send_trailing_stop(
                                tsl,
                                trade.ts())
                        .subscribe(
                                m_event_loop.m_event_loop,
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
        Logger::log<LogLevel::Debug>("Trailing stop loss removed");
        return;
    }

    StopLoss sl{
            tsl->symbol_name(),
            tsl->m_active_stop_loss_price.value_or(0),
            tsl->side()};

    m_trailing_stop_channel.push(tsl->m_update_ts, sl);
}
