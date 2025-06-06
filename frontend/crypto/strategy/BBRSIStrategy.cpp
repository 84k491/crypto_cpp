#include "BBRSIStrategy.h"

#include "EventLoopSubscriber.h"
#include "Macros.h"

BBRSIStrategyConfig::BBRSIStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("margin")) {
        m_margin = json.get()["margin"].get<unsigned>();
    }
    if (json.get().contains("std_dev")) {
        m_std_deviation_coefficient = json.get()["std_dev"].get<double>();
    }
    if (json.get().contains("bb_interval")) {
        m_bb_interval = json.get()["bb_interval"].get<unsigned>();
    }
    if (json.get().contains("rsi_interval")) {
        m_rsi_interval = json.get()["rsi_interval"].get<unsigned>();
    }
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }
}

bool BBRSIStrategyConfig::is_valid() const
{
    return m_timeframe > std::chrono::seconds(0) &&
            m_bb_interval > 0 &&
            m_rsi_interval > 0 &&
            m_std_deviation_coefficient > 0. &&
            m_margin > 0;
}

JsonStrategyConfig BBRSIStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["margin"] = m_margin;
    json["bb_interval"] = m_bb_interval;
    json["rsi_interval"] = m_rsi_interval;
    json["std_dev"] = m_std_deviation_coefficient;
    json["timeframe_s"] = std::chrono::duration_cast<std::chrono::seconds>(m_timeframe).count();
    return json;
}

BBRSIStrategy::BBRSIStrategy(
        BBRSIStrategyConfig config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders)
    , m_config(config)
    , m_bollinger_bands(config.m_timeframe * config.m_bb_interval, config.m_std_deviation_coefficient)
    , m_rsi_top_threshold(100 - config.m_margin)
    , m_rsi(config.m_rsi_interval)
    , m_rsi_bot_threshold(config.m_margin)
{
    m_channel_subs.push_back(channels.candle_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto &, const Candle & candle) {
                push_candle(candle);
            }));
}

void BBRSIStrategy::push_candle(const Candle & candle)
{
    const auto ts = candle.close_ts();
    const auto price = candle.close();

    const auto rsi = m_rsi.push_candle(candle);

    UNWRAP_RET_VOID(bb_res, m_bollinger_bands.push_value({ts, price}));

    m_strategy_internal_data_channel.push(ts, {"prices", "upper_band", bb_res.m_upper_band});
    m_strategy_internal_data_channel.push(ts, {"prices", "trend", bb_res.m_trend});
    m_strategy_internal_data_channel.push(ts, {"prices", "lower_band", bb_res.m_lower_band});

    if (!rsi.has_value()) {
        return;
    }

    m_strategy_internal_data_channel.push(ts, {"rsi", "upper", m_rsi_top_threshold});
    m_strategy_internal_data_channel.push(ts, {"rsi", "rsi", rsi.value()});
    m_strategy_internal_data_channel.push(ts, {"rsi", "lower", m_rsi_bot_threshold});

    if (m_last_signal_side) {
        const auto last_signal_side = m_last_signal_side.value();
        switch (last_signal_side.value()) {
        case SideEnum::Buy: {
            if (price > bb_res.m_trend) {
                m_last_signal_side = {};
            }
            break;
        }
        case SideEnum::Sell: {
            if (price < bb_res.m_trend) {
                m_last_signal_side = {};
            }
            break;
        }
        }
        return;
    }

    if (price > bb_res.m_upper_band && candle.side() == SideEnum::Buy) {
        m_price_in_trigger_zone_bb = true;
        return;
    }
    if (price < bb_res.m_lower_band && candle.side() == SideEnum::Sell) {
        m_price_in_trigger_zone_bb = true;
        return;
    }

    if (!m_price_in_trigger_zone_bb) {
        // it's not a trend reverse
        return;
    }
    m_price_in_trigger_zone_bb = false;

    if (bb_res.m_trend < price && rsi.value() > m_rsi_top_threshold && candle.side() == SideEnum::Sell) {
        try_send_order(Side::sell(), price, ts, {});
        m_last_signal_side = Side::sell();
        return;
    }
    if (bb_res.m_trend > price && rsi.value() < m_rsi_bot_threshold && candle.side() == SideEnum::Buy) {
        try_send_order(Side::buy(), price, ts, {});
        m_last_signal_side = Side::buy();
        return;
    }
}

bool BBRSIStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> BBRSIStrategy::timeframe() const
{
    return m_config.m_timeframe;
}
