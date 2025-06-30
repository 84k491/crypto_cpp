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

double GridStrategyConfig::get_one_level_width(double ref_price) const
{
    const auto price_rad = m_price_radius_perc * ref_price;
    return price_rad / m_levels_per_side;
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
    , m_event_loop(event_loop)
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
int GridStrategy::get_level_number(double price) const
{
    const double diff = price - m_last_trend_value;
    return (int)std::lround(diff / m_config.get_one_level_width(m_last_trend_value));
}

double GridStrategy::get_price_from_level_number(int level_num) const
{
    return m_last_trend_value + (level_num * m_config.get_one_level_width(m_last_trend_value));
}

void GridStrategy::push_price(std::chrono::milliseconds ts, double price)
{
    {
        UNWRAP_RET_VOID(v, m_trend.push_value({ts, price}))
        m_last_trend_value = v;
    }

    const auto price_level = get_level_number(price);

    // TODO handle 'over 2 levels' scenario

    if (price_level == 0 || m_orders_by_levels.contains(price_level)) {
        return;
    }

    const Side side = price_level > 0 ? Side::sell() : Side::buy();
    auto & channel = m_orders.send_market_order(
            price,
            SignedVolume{UnsignedVolume::from(m_pos_currency_amount).value(), side},
            ts);

    auto sub = channel.subscribe(
            m_event_loop.m_event_loop,
            [&](const std::shared_ptr<MarketOrder> & or_ptr) {
                if (or_ptr->filled_volume().value() > 0.) {
                    on_order_traded(*or_ptr, price_level);
                }

                // TODO handle reject
            });

    m_orders_by_levels.emplace(
            price_level,
            Level{
                    .level_num = price_level,
                    .mo_sub = sub,
                    .market_order = channel.get(),
                    .tp = std::nullopt,
                    .sl = std::nullopt});
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

    const auto [tp_price, sl_price] = calc_tp_sl_prices(order.price(), order.side()); // TODO use trade price
    auto vol = SignedVolume{order.filled_volume(), order.side().opposite()};
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

GridStrategy::Level * GridStrategy::find_level(xg::Guid order_guid)
{
    for (auto it = m_orders_by_levels.begin(), end = m_orders_by_levels.end(); it != end; ++it) {
        auto & [l, orders] = *it;
        auto & [_, _1, mo, tp, sl] = orders;

        if (tp.has_value() && tp->guid() == order_guid) {
            return &orders;
        }

        if (sl.has_value() && sl->guid() == order_guid) {
            return &orders;
        }
    }

    return nullptr;
}

void GridStrategy::on_take_profit_active(const TakeProfitMarketOrder & tp)
{
    auto * level = find_level(tp.guid());
    if (level == nullptr) {
        Logger::logf<LogLevel::Error>("Can't find level for accepted tp: {}", tp.guid());
        return;
    }

    level->tp = tp;
}

void GridStrategy::on_stop_loss_active(const StopLossMarketOrder & sl)
{
    auto * level = find_level(sl.guid());
    if (level == nullptr) {
        Logger::logf<LogLevel::Error>("Can't find level for accepted sl: {}", sl.guid());
        return;
    }

    level->sl = sl;

    m_orders_by_levels.erase(level->level_num);
}

void GridStrategy::on_take_profit_inactive(const TakeProfitMarketOrder & order)
{
    const auto * level = find_level(order.guid());
    if (level == nullptr) {
        Logger::logf<LogLevel::Error>("Can't find level for tp: {}", order.guid());
        return;
    }

    m_orders.cancel_stop_loss(level->sl->guid());

    m_orders_by_levels.erase(level->level_num);
}

void GridStrategy::on_stop_loss_inactive(const StopLossMarketOrder & order)
{
    const auto * level = find_level(order.guid());
    if (level == nullptr) {
        Logger::logf<LogLevel::Error>("Can't find level for sl: {}", order.guid());
        return;
    }

    m_orders.cancel_take_profit(level->sl->guid());

    m_orders_by_levels.erase(level->level_num);
}

GridStrategy::TpSlPrices GridStrategy::calc_tp_sl_prices(double order_price, Side side) const
{
    const auto current_level = get_level_number(order_price);
    const auto tp_level = current_level + side.sign();
    const auto tp_price = get_price_from_level_number(tp_level);

    // top of the last level
    const auto sl_price = (m_config.m_levels_per_side + 1) * (-1 * side.sign()) * m_config.get_one_level_width(m_last_trend_value);

    return {.take_profit_price = tp_price, .stop_loss_price = sl_price};
}
