#include "CandleBollingerBandsStrategy.h"

#include "EventLoopSubscriber.h"
#include "Macros.h"
#include "StrategyBase.h"

#include <algorithm>

CandleBollingerBandsStrategyConfig::CandleBollingerBandsStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("std_dev")) {
        m_std_deviation_coefficient = json.get()["std_dev"].get<double>();
    }
    if (json.get().contains("interval_m")) {
        m_interval = std::chrono::minutes(json.get()["interval_m"].get<int>());
    }
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }
    if (json.get().contains("candles_threshold")) {
        m_candles_threshold = json.get()["candles_threshold"].get<int>();
    }
}

bool CandleBollingerBandsStrategyConfig::is_valid() const
{
    return m_interval > std::chrono::minutes(0) &&
            m_std_deviation_coefficient > 0. &&
            m_interval > m_timeframe;
}

JsonStrategyConfig CandleBollingerBandsStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["std_dev"] = m_std_deviation_coefficient;
    json["interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_interval).count();
    json["timeframe_s"] = std::chrono::duration_cast<std::chrono::seconds>(m_timeframe).count();
    json["candles_threshold"] = m_candles_threshold;
    return json;
}

CandleBollingerBandsStrategy::CandleBollingerBandsStrategy(
        const CandleBollingerBandsStrategyConfig & config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders)
    , m_config(config)
    , m_bollinger_bands(config.m_interval, config.m_std_deviation_coefficient)
{
    m_channel_subs.push_back(channels.candle_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto &, const Candle & candle) {
                push_candle(candle);
            }));
}

bool CandleBollingerBandsStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> CandleBollingerBandsStrategy::timeframe() const
{
    return {};
}

void CandleBollingerBandsStrategy::push_candle(const Candle & candle) {
    const auto ts = candle.close_ts();
    const auto price = candle.close();

    UNWRAP_RET_VOID(bb_res, m_bollinger_bands.push_value({ts, price}));

    m_strategy_internal_data_channel.push(ts, {"prices", "upper_band", bb_res.m_upper_band});
    m_strategy_internal_data_channel.push(ts, {"prices", "trend", bb_res.m_trend});
    m_strategy_internal_data_channel.push(ts, {"prices", "lower_band", bb_res.m_lower_band});

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
        m_candles_above_price_trigger = std::max(m_candles_above_price_trigger, 0);
        ++m_candles_above_price_trigger;
        return;
    }
    if (price < bb_res.m_lower_band && candle.side() == SideEnum::Sell) {
        m_candles_above_price_trigger = std::min(m_candles_above_price_trigger, 0);
        --m_candles_above_price_trigger;
        return;
    }

    if (m_candles_above_price_trigger == 0) {
        // it's not a trend reverse
        return;
    }

    const bool too_many_candles_above = std::abs(m_candles_above_price_trigger) > m_config.m_candles_threshold;
    if (!too_many_candles_above && m_candles_above_price_trigger > 0 && candle.side() == SideEnum::Sell) {
        try_send_order(Side::sell(), price, ts, {});
        m_last_signal_side = Side::sell();
    }
    if (!too_many_candles_above && m_candles_above_price_trigger < 0 && candle.side() == SideEnum::Buy) {
        try_send_order(Side::buy(), price, ts, {});
        m_last_signal_side = Side::buy();
    }

    // Too many candles above trigger price
    m_candles_above_price_trigger = 0;
}
