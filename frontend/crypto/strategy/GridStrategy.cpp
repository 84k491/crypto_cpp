#include "GridStrategy.h"

#include "Logger.h"
#include "Macros.h"
#include "StrategyBase.h"

#include <cmath>

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
    const auto price_radius = (m_price_radius_perc * 0.01) * ref_price;
    return price_radius / m_levels_per_side;
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
int GridLevels::get_level_number(double price, double trend, double level_width)
{
    const double diff = price - trend;
    const double dbl_num = diff/ level_width;
    if (dbl_num > 0) {
        return int(floorl(dbl_num));
    }
    else {
        return int(ceill(dbl_num));
    }
}

double GridLevels::get_price_from_level_number(int level_num, double trend, double level_width)
{
    double offset = level_num * level_width;
    return trend + offset;
}

int GridStrategy::get_level_number(double price) const
{
    return GridLevels::get_level_number(
            price,
            m_last_trend_value,
            m_config.get_one_level_width(m_last_trend_value));
}

double GridStrategy::get_price_from_level_number(int level_num) const
{
    return GridLevels::get_price_from_level_number(
            level_num,
            m_last_trend_value,
            m_config.get_one_level_width(m_last_trend_value));
}

void GridStrategy::push_price(std::chrono::milliseconds ts, double price)
{
    {
        UNWRAP_RET_VOID(v, m_trend.push_value({ts, price}))
        m_last_trend_value = v;
        m_strategy_internal_data_channel.push(ts, {"prices", "trend", v});
    }
    maybe_report_levels(ts);

    const auto price_level = get_level_number(price);
    // TODO handle 'over 2 levels' scenario

    if (price_level == 0) {
        return;
    }
    if (unsigned(std::abs(price_level)) >= m_config.m_levels_per_side) {
        return;
    }
    if (m_orders_by_levels.contains(price_level)) {
        return;
    }

    const Side side = price_level > 0 ? Side::sell() : Side::buy();
    auto & channel = m_orders.send_market_order(
            price,
            SignedVolume{UnsignedVolume::from(m_pos_currency_amount).value(), side},
            ts);

    auto sub = channel.subscribe(
            m_event_loop.m_event_loop,
            [&, price_level](const std::shared_ptr<MarketOrder> & or_ptr) {
                if (or_ptr->status() == OrderStatus::Filled) {
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
                    .tp_sub = nullptr,
                    .tp = nullptr,
                    .sl_sub = nullptr,
                    .sl = nullptr});
}

void GridStrategy::on_order_traded(const MarketOrder & order, int price_level)
{
    const auto it = m_orders_by_levels.find(price_level);
    if (it == m_orders_by_levels.end()) {
        Logger::logf<LogLevel::Error>("Can't find orders for price level {}", price_level);
        // TODO panic?
        return;
    }
    auto & orders = it->second;

    // TODO verify volume

    if (orders.tp || orders.sl) {
        Logger::logf<LogLevel::Error>("There already are tp or sl for level {}", price_level);
        // TODO panic?
        return;
    }

    const auto [tp_price, sl_price] = calc_tp_sl_prices(order.price(), order.side());
    auto vol = SignedVolume{order.filled_volume(), order.side().opposite()};

    {
        auto & take_profit_channel = m_orders.send_take_profit(
                tp_price,
                vol,
                order.signal_ts());
        const auto tp_sub = take_profit_channel.subscribe(
                m_event_loop.m_event_loop,
                [&, price_level](const std::shared_ptr<TakeProfitMarketOrder> & tp) {
                    switch (tp->status()) {
                    case OrderStatus::Rejected: {
                        Logger::logf<LogLevel::Error>("Tp rejected: {}", tp->reject_reason());
                        // TODO forward reject
                        break;
                    }
                    case OrderStatus::Filled: {
                        on_take_profit_traded(*tp, price_level);
                        break;
                    }
                    default: break;
                    }
                });
        orders.tp = take_profit_channel.get();
        orders.tp_sub = tp_sub;
    }

    {
        auto & stop_loss_channel = m_orders.send_stop_loss(
                sl_price,
                vol,
                order.signal_ts());
        const auto sl_sub = stop_loss_channel.subscribe(
                m_event_loop.m_event_loop,
                [&, price_level](const std::shared_ptr<StopLossMarketOrder> & sl) {
                    switch (sl->status()) {
                    case OrderStatus::Rejected: {
                        Logger::logf<LogLevel::Error>("Sl rejected: {}", sl->reject_reason());
                        // TODO forward reject
                        break;
                    }
                    case OrderStatus::Filled: {
                        on_stop_loss_traded(*sl, price_level);
                        break;
                    }
                    default: break;
                    }
                });
        orders.sl = stop_loss_channel.get();
        orders.sl_sub = sl_sub;
    }
}

void GridStrategy::on_take_profit_traded(const TakeProfitMarketOrder & order, int price_level)
{
    const auto it = m_orders_by_levels.find(price_level);
    if (it == m_orders_by_levels.end()) {
        Logger::logf<LogLevel::Error>("Can't find level for sl: {}", order.guid());
        return;
    }
    auto & level = it->second;

    m_orders.cancel_stop_loss(level.sl->guid());

    m_orders_by_levels.erase(level.level_num);
}

void GridStrategy::on_stop_loss_traded(const StopLossMarketOrder & order, int price_level)
{
    const auto it = m_orders_by_levels.find(price_level);
    if (it == m_orders_by_levels.end()) {
        Logger::logf<LogLevel::Error>("Can't find level for sl: {}", order.guid());
        return;
    }
    auto & level = it->second;

    m_orders.cancel_take_profit(level.tp->guid());

    m_orders_by_levels.erase(level.level_num);
}

GridStrategy::TpSlPrices GridStrategy::calc_tp_sl_prices(double order_price, Side side) const
{
    const auto current_level = get_level_number(order_price);
    const auto tp_level = current_level + side.sign();
    const auto tp_price = get_price_from_level_number(tp_level);

    // top of the last level
    const auto top_level = int(m_config.m_levels_per_side + 1);
    const auto sign = (-1 * side.sign());
    const auto level_width = m_config.get_one_level_width(m_last_trend_value);
    const auto sl_price = (top_level * sign * level_width) + order_price;

    return {.take_profit_price = tp_price, .stop_loss_price = sl_price};
}

void GridStrategy::maybe_report_levels(std::chrono::milliseconds ts)
{
    if (ts - last_reported_ts < m_config.m_interval / 10) {
        return;
    }

    for (int i = int(m_config.m_levels_per_side) * -1; i < int(m_config.m_levels_per_side) + 1; ++i) {
        const auto p = get_price_from_level_number(i);
        m_strategy_internal_data_channel.push(ts, {"prices", std::to_string(i), p});
    }

    last_reported_ts = ts;
}

void GridStrategy::print_levels()
{
    std::stringstream ss;
    for (int i = int(m_config.m_levels_per_side) * -1; i < int(m_config.m_levels_per_side) + 1; ++i) {
        const auto p = get_price_from_level_number(i);
        ss << fmt::format("Level {}: {}, ", i, p);
    }
    Logger::logf<LogLevel::Debug>("{}", ss.str());
}
