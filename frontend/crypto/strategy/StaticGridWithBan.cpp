#include "StaticGridWithBan.h"

#include "GridLevels.h"
#include "Logger.h"
#include "ScopeExit.h"

#include <algorithm>

StaticGridWithBanStrategyConfig::StaticGridWithBanStrategyConfig(JsonStrategyConfig json)
{
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }
    if (json.get().contains("interval")) {
        m_interval = json.get()["interval"].get<int>();
    }
    if (json.get().contains("level_width_perc")) {
        m_level_width_perc = json.get()["level_width_perc"].get<decltype(m_level_width_perc)>();
    }
}

bool StaticGridWithBanStrategyConfig::is_valid() const
{
    return true;
}

JsonStrategyConfig StaticGridWithBanStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["timeframe_s"] = std::chrono::duration_cast<std::chrono::seconds>(m_timeframe).count();
    json["interval"] = m_interval;
    json["level_width_perc"] = m_level_width_perc;
    return json;
}

double StaticGridWithBan::level_width_in_currency() const
{
    return (m_previous_limits->mid_price() / 100.) * m_config.m_level_width_perc;
}

StaticGridWithBan::StaticGridWithBan(
        const StaticGridWithBanStrategyConfig & config,
        EventLoop & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_config(config)
    , m_event_loop(event_loop)
    , m_orders(orders)
    , m_sub{event_loop}
{
    m_sub.subscribe(
            channels.candle_channel,
            [](const auto &) {},
            [this](const auto & ts, const Candle & candle) {
                push_candle(ts, candle);
            });
}

bool StaticGridWithBan::is_valid() const
{
    return m_config.m_level_width_perc > 0.;
}

std::optional<std::chrono::milliseconds> StaticGridWithBan::timeframe() const
{
    return m_config.m_timeframe;
}

double StaticGridWithBan::Limits::mid_price() const
{
    return min_price + ((max_price - min_price) / 2.);
}

void StaticGridWithBan::Limits::update_prices(double price)
{
    min_price = std::min(min_price, price);
    max_price = std::max(max_price, price);
}

double StaticGridWithBan::Limits::price_radius() const
{
    return max_price - mid_price();
}

int StaticGridWithBan::get_level_number(double price) const
{
    return GridLevels::get_level_number(
            price,
            m_previous_limits->mid_price(),
            level_width_in_currency());
}

double StaticGridWithBan::get_price_from_level_number(int level_num) const
{
    return GridLevels::get_price_from_level_number(
            level_num,
            m_previous_limits->mid_price(),
            level_width_in_currency());
}

int StaticGridWithBan::get_levels_per_side() const
{
    const auto radius = m_previous_limits->max_price - m_previous_limits->mid_price();
    const double levels_count = radius / level_width_in_currency();
    return std::floor(levels_count);
}

void StaticGridWithBan::try_interval_handover(std::chrono::milliseconds ts)
{
    if (ts < m_current_interval_start_ts + m_config.m_interval * m_config.m_timeframe) {
        return;
    }
    m_current_interval_start_ts = ts;

    report_levels(ts - m_config.m_timeframe);

    m_previous_limits = m_next_limits;
    m_next_limits = {};

    LOG_INFO("Handover: PrevMin: {}, PrevMax: {}", m_previous_limits->min_price, m_previous_limits->max_price);
    report_levels(ts);
}

void StaticGridWithBan::push_candle(std::chrono::milliseconds ts, const Candle & candle)
{
    if (m_current_interval_start_ts == std::chrono::milliseconds{}) {
        m_current_interval_start_ts = ts;
    }

    try_interval_handover(ts);

    const bool new_banned_state = is_banned(ts, candle);
    ScopeExit se{[&] {
        m_prev_banned_state = new_banned_state;
    }};

    const auto price = candle.close();
    m_next_limits.update_prices(price);

    if (!m_previous_limits.has_value()) {
        return;
    }

    const auto price_level = get_level_number(price);

    if (price_level == 0) {
        return;
    }
    if (price < m_previous_limits->min_price || m_previous_limits->max_price < price) {
        m_last_out_of_bounds_price_ts = ts;
        return;
    }
    if (new_banned_state) {
        return;
    }
    if (m_orders_by_levels.contains(price_level)) {
        return;
    }

    const Side side = price_level > 0 ? Side::sell() : Side::buy();
    const auto default_size_opt = UnsignedVolume::from(m_pos_currency_amount / price);
    if (!default_size_opt.has_value()) {
        LOG_ERROR("Can't get proper order volume. Amount: {}, price: {}", m_pos_currency_amount, price);
        // TODO push to error channel
        return;
    }
    auto & channel = m_orders.send_market_order(
            price,
            SignedVolume{default_size_opt.value(), side},
            ts);

    auto sub = EventSubcriber{m_event_loop};
    sub.subscribe(
            channel,
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

void StaticGridWithBan::on_order_traded(const MarketOrder & order, int price_level)
{
    const auto it = m_orders_by_levels.find(price_level);
    if (it == m_orders_by_levels.end()) {
        LOG_ERROR("Can't find orders for price level {}", price_level);
        // TODO panic?
        return;
    }
    auto & orders = it->second;

    // TODO verify volume

    if (orders.tp || orders.sl) {
        LOG_ERROR("There already are tp or sl for level {}", price_level);
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

        auto tp_sub = std::make_unique<EventSubcriber>(m_event_loop);
        tp_sub->subscribe(
                take_profit_channel,
                [&, price_level](const std::shared_ptr<TakeProfitMarketOrder> & tp) {
                    switch (tp->status()) {
                    case OrderStatus::Rejected: {
                        LOG_ERROR("Tp rejected: {}", tp->reject_reason());
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
        orders.tp_sub = std::move(tp_sub);
    }

    {
        auto & stop_loss_channel = m_orders.send_stop_loss(
                sl_price,
                vol,
                order.signal_ts());
        auto sl_sub = std::make_unique<EventSubcriber>(m_event_loop);
        sl_sub->subscribe(
                stop_loss_channel,
                [&, price_level](const std::shared_ptr<StopLossMarketOrder> & sl) {
                    switch (sl->status()) {
                    case OrderStatus::Rejected: {
                        LOG_ERROR("Sl rejected: {}", sl->reject_reason());
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
        orders.sl_sub = std::move(sl_sub);
    }
}

void StaticGridWithBan::on_take_profit_traded(const TakeProfitMarketOrder & order, int price_level)
{
    const auto it = m_orders_by_levels.find(price_level);
    if (it == m_orders_by_levels.end()) {
        LOG_ERROR("Can't find level for sl: {}", order.guid());
        return;
    }
    auto & level = it->second;

    m_orders.cancel_stop_loss(level.sl->guid());

    m_orders_by_levels.erase(level.level_num);
}

void StaticGridWithBan::on_stop_loss_traded(const StopLossMarketOrder & order, int price_level)
{
    const auto it = m_orders_by_levels.find(price_level);
    if (it == m_orders_by_levels.end()) {
        LOG_ERROR("Can't find level for sl: {}", order.guid());
        return;
    }
    auto & level = it->second;

    m_orders.cancel_take_profit(level.tp->guid());

    m_orders_by_levels.erase(level.level_num);
}

StaticGridWithBan::TpSlPrices StaticGridWithBan::calc_tp_sl_prices(double order_price, Side side) const
{
    const auto current_level = get_level_number(order_price);
    const auto tp_level = current_level + side.sign();
    const auto tp_price = get_price_from_level_number(tp_level);

    // top of the last level
    const auto sl_price = [&] {
        if (side == Side::buy()) {
            return m_previous_limits->min_price - level_width_in_currency();
        }
        else {
            return m_previous_limits->max_price + level_width_in_currency();
        }
    }();

    return {.take_profit_price = tp_price, .stop_loss_price = sl_price};
}

void StaticGridWithBan::report_levels(std::chrono::milliseconds ts)
{
    if (!m_previous_limits) {
        return;
    }

    const int levels_per_side = get_levels_per_side();
    m_max_levels_per_side = std::max(levels_per_side, m_max_levels_per_side);

    for (int i = m_max_levels_per_side * -1; i < m_max_levels_per_side + 1; ++i) {
        const auto p = (std::abs(i) > levels_per_side) ? NAN : get_price_from_level_number(i);
        m_strategy_internal_data_channel.push(ts, {.chart_name = "prices", .series_name = std::to_string(i), .value = p});
    }
}
