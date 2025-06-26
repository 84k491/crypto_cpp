#include "GridStrategy.h"

#include "Logger.h"
#include "Macros.h"
#include "StrategyBase.h"

GridStrategyConfig::GridStrategyConfig(JsonStrategyConfig json)
{
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }
    if (json.get().contains("interval_m")) {
        m_interval = std::chrono::minutes(json.get()["interval_m"].get<int>());
    }
    if (json.get().contains("levels_per_side")) {
        m_levels_per_side = json.get()["levels_per_side"].get<decltype(m_levels_per_side)>();
    }
    if (json.get().contains("price_radius_perc")) {
        m_price_radius_perc = json.get()["price_radius_perc"].get<decltype(m_price_radius_perc)>();
    }
}

bool GridStrategyConfig::is_valid() const
{
    return m_levels_per_side > 0;
}

JsonStrategyConfig GridStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["timeframe_s"] = std::chrono::duration_cast<std::chrono::seconds>(m_timeframe).count();
    json["interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_interval).count();
    json["levels_per_side"] = m_levels_per_side;
    json["price_radius_perc"] = m_price_radius_perc;
    return json;
}

GridStrategy::GridStrategy(
        const GridStrategyConfig & config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders)
    , m_config(config)
    , m_orders(orders)
    , m_trend(config.m_interval)
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
    return m_config.m_timeframe;
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
    m_orders_by_levels.emplace(
            price_level,
            Level{price_level, SignedVolume{m_pos_currency_amount}, std::nullopt, std::nullopt});
    send_order(side, price, ts, [&](const MarketOrder & mo, bool active) {
        if (!active) {
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

    // auto vol = SignedVolume{order.volume().value(), order.side().opposite()};
    auto vol = SignedVolume{0.};
    const auto tp_price = 0.; // TODO calculate ?
    m_orders.send_take_profit(
            tp_price,
            vol,
            order.signal_ts(),
            [&](const TakeProfitMarketOrder & tp, bool active) {
                if (active) {
                    on_take_profit_accepted(tp);
                    return;
                }

                on_take_profit_traded(tp);
                // TODO handle reject
            });

    const auto sl_price = 0.; // TODO calculate ?
    m_orders.send_stop_loss(
            sl_price,
            vol,
            order.signal_ts(),
            [&](const StopLossMarketOrder & sl, bool active) {
                if (active) {
                    on_stop_loss_accepted(sl);
                    return;
                }

                on_stop_loss_traded(sl);
                // TODO handle reject
            });
}

void GridStrategy::on_take_profit_traded(TakeProfitMarketOrder order)
{
    std::map<int, GridStrategy::Level>::iterator lvl_it = m_orders_by_levels.end();
    for (auto it = m_orders_by_levels.begin(), end = m_orders_by_levels.end(); it != end; ++it) {
        const auto & [l, orders] = *it;
        if (!orders.tp.has_value()) {
            continue;
        }
        const auto & tp = orders.tp.value();

        if (tp.guid() != order.guid()) {
            continue;
        }

        lvl_it = it;
        break;
    }
    const auto & [lvl, orders] = *lvl_it;
    const auto & [_, vol, _tp, sl] = orders;

    // TODO close vol, cancel sl
    const auto price = 0.;                // TODO
    const std::chrono::milliseconds ts{}; // TODO add in event

    // closing volume
    send_order(vol.as_unsigned_and_side().second, price, ts, {});

    m_orders.cancel_stop_loss(sl->guid());
}

void GridStrategy::on_stop_loss_traded(StopLossMarketOrder order)
{
    std::map<int, GridStrategy::Level>::iterator lvl_it = m_orders_by_levels.end();
    for (auto it = m_orders_by_levels.begin(), end = m_orders_by_levels.end(); it != end; ++it) {
        const auto & [l, orders] = *it;
        if (!orders.sl.has_value()) {
            continue;
        }
        const auto & sl = orders.sl.value();

        if (sl.guid() != order.guid()) {
            continue;
        }

        lvl_it = it;
        break;
    }
    const auto & [lvl, orders] = *lvl_it;
    const auto & [_, vol, tp, _sl] = orders;

    // TODO close vol, cancel sl
    const auto price = 0.;                // TODO
    const std::chrono::milliseconds ts{}; // TODO add in event

    // closing volume
    send_order(vol.as_unsigned_and_side().second, price, ts, {});

    m_orders.cancel_take_profit(tp->guid());
}
