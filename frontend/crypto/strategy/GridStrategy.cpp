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
            on_order_traded(mo, price_level);
        }

        // TODO handle reject
    });
}

void GridStrategy::on_order_traded(const MarketOrder & order, int price_level)
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

    const auto [tp_price, sl_price] = calc_tp_sl_prices(order.price()); // TODO use trade price
    auto vol = SignedVolume{order.volume(), order.side().opposite()};
    m_orders.send_take_profit(
            tp_price,
            vol,
            order.signal_ts(),
            [&](const TakeProfitMarketOrder & tp, bool active) {
                if (active) {
                    on_take_profit_active(tp);
                    return;
                }

                on_take_profit_inactive(tp);
                // TODO handle reject
            });

    m_orders.send_stop_loss(
            sl_price,
            vol,
            order.signal_ts(),
            [&](const StopLossMarketOrder & sl, bool active) {
                if (active) {
                    on_stop_loss_active(sl);
                    return;
                }

                on_stop_loss_inactive(sl);
                // TODO handle reject
            });
}

GridStrategy::Level * GridStrategy::find_level(xg::Guid guid)
{
    for (auto it = m_orders_by_levels.begin(), end = m_orders_by_levels.end(); it != end; ++it) {
        auto & [l, orders] = *it;
        auto & [_, vol, tp, sl] = orders;

        if (tp.has_value() && tp->guid() == guid) {
            return &orders;
        }

        if (sl.has_value() && sl->guid() == guid) {
            return &orders;
        }
    }

    return nullptr;
}

void GridStrategy::on_take_profit_active(const TakeProfitMarketOrder & tp)
{
    auto * level = find_level(tp.guid());
    if (!level) {
        Logger::logf<LogLevel::Error>("Can't find level for accepted tp: {}", tp.guid());
        return;
    }

    level->tp = tp;
}

void GridStrategy::on_stop_loss_active(const StopLossMarketOrder & sl)
{
    auto * level = find_level(sl.guid());
    if (!level) {
        Logger::logf<LogLevel::Error>("Can't find level for accepted sl: {}", sl.guid());
        return;
    }

    level->sl = sl;

    m_orders_by_levels.erase(level->level_num);
}

void GridStrategy::on_take_profit_inactive(TakeProfitMarketOrder order)
{
    const auto * level = find_level(order.guid());
    if (!level) {
        Logger::logf<LogLevel::Error>("Can't find level for tp: {}", order.guid());
        return;
    }

    m_orders.cancel_stop_loss(level->sl->guid());

    m_orders_by_levels.erase(level->level_num);
}

void GridStrategy::on_stop_loss_inactive(StopLossMarketOrder order)
{
    const auto * level = find_level(order.guid());
    if (!level) {
        Logger::logf<LogLevel::Error>("Can't find level for sl: {}", order.guid());
        return;
    }

    m_orders.cancel_take_profit(level->sl->guid());

    // TODO vol must be closed, remove the whole level
}

GridStrategy::TpSlPrices GridStrategy::calc_tp_sl_prices(double ref_price) const
{
    // TODO
    return {0., 0.};
}
