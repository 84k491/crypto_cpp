#include "GridStrategy.h"

#include "Logger.h"
#include "Macros.h"
#include "StrategyBase.h"

GridStrategy::GridStrategy(
        const GridStrategyConfig & config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders)
    , m_config(config)
    , m_trend(std::chrono::minutes(30)) // TODO
{
    m_channel_subs.push_back(channels.price_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto & ts, const double & price) {
                push_price(ts, price);
            }));
}

bool GridStrategy::is_valid() const
{
    return true;
}

std::optional<std::chrono::milliseconds> GridStrategy::timeframe() const
{
    return std::chrono::minutes{1}; // TODO
}

//   ...
// ------------------------ +2 * width
//   1 lvl
// ------------------------ +1 * width
//   0 lvl
// ------------------------ ref
//   0 lvl
// ------------------------ -1 * width
//   -1 lvl
// ------------------------ -2 * width
//   ...
int get_price_level_number(double ref, double price, double lvl_width)
{
    const double diff = price - ref;
    return diff / lvl_width;
}

void GridStrategy::push_price(std::chrono::milliseconds ts, double price)
{
    {
        UNWRAP_RET_VOID(v, m_trend.push_value({ts, price}))
        m_last_trend_value = v;
    }

    constexpr double lvl_width = 10; // TODO
    const auto price_level = get_price_level_number(m_last_trend_value, price, lvl_width);

    // TODO handle 'over 2 levels' scenario

    if (price_level == 0 || m_orders_by_levels.contains(price_level)) {
        return;
    }

    const Side side = price_level > 0 ? Side::sell() : Side::buy();
    // TODO save order in m_orders_by_levels;
    send_order(side, price, ts, [&](const MarketOrder & mo, bool ok) {
        if (!ok) {
            Logger::log<LogLevel::Error>("Market order reject");
            // TODO panic?
            return;
        }

        on_order_accepted(mo, price_level);
    });
}

void GridStrategy::on_order_accepted(const MarketOrder & order, int price_level)
{
    const auto it = m_orders_by_levels.find(price_level);
    if (it == m_orders_by_levels.end()) {
        Logger::logf<LogLevel::Error>("Can't find orders for price level {}", price_level);
        // TODO panic?
        return;
    }
    const auto & orders = it->second;

    // TODO verify volume

    if (orders.tp.has_value() || orders.sl.has_value()) {
        Logger::logf<LogLevel::Error>("There already are tp or sl for level {}", price_level);
        // TODO panic?
        return;
    }

    const auto tp_price = 0.; // TODO calculate
    send_take_profit(
            order.side().opposite(),
            tp_price,
            order.signal_ts(),
            [&](const TakeProfitMarketOrder & tp, bool ok) {
                if (ok) {
                    Logger::log<LogLevel::Error>("Take profit reject");
                    // TODO panic?
                    return;
                }

                on_take_profit_accepted(tp);
            });

    const auto sl_price = 0.; // TODO calculate
    send_stop_loss(
            order.side().opposite(),
            sl_price,
            order.signal_ts(),
            [&](const StopLossMarketOrder & sl, bool ok) {
                if (ok) {
                    Logger::log<LogLevel::Error>("Take profit reject");
                    // TODO panic?
                    return;
                }

                on_stop_loss_accepted(sl);
            });
}

void GridStrategy::on_take_profit_traded(TakeProfitUpdatedEvent ev)
{
    if (ev.active) { // TODO it can be just cancelled, add new flag for traded (last status may be?)
        Logger::logf<LogLevel::Error>("Active tp on traded, guid: {}", ev.guid);
        return;
    }

    for (const auto & [lvl, order] : m_orders_by_levels) {
        if (!order.tp.has_value()) {
            continue;
        }
        const auto & tp = order.tp.value();

        if (tp.guid() != ev.guid) {
            continue;
        }

        // TODO close vol, cancel sl
    }
}
