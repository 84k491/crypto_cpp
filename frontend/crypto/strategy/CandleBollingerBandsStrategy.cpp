#include "CandleBollingerBandsStrategy.h"

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

CandleBollingerBandsStrategy::CandleBollingerBandsStrategy(const CandleBollingerBandsStrategyConfig & config)
    : m_config(config)
    , m_bollinger_bands(config.m_interval, config.m_std_deviation_coefficient)
{
}

bool CandleBollingerBandsStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> CandleBollingerBandsStrategy::timeframe() const
{
    return {};
}

std::optional<Signal> CandleBollingerBandsStrategy::push_candle(const Candle & candle)
{
    const auto ts = candle.close_ts();
    const auto price = candle.close();

    const auto bb_res_opt = m_bollinger_bands.push_value({ts, price});
    UNWRAP_RET(bb_res, std::nullopt);

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
        return std::nullopt;
    }

    if (price > bb_res.m_upper_band && candle.side() == SideEnum::Buy) {
        m_candles_above_price_trigger = std::max(m_candles_above_price_trigger, 0);
        ++m_candles_above_price_trigger;
        return std::nullopt;
    }
    if (price < bb_res.m_lower_band && candle.side() == SideEnum::Sell) {
        m_candles_above_price_trigger = std::min(m_candles_above_price_trigger, 0);
        --m_candles_above_price_trigger;
        return std::nullopt;
    }

    if (m_candles_above_price_trigger == 0) {
        // it's not a trend reverse
        return std::nullopt;
    }

    const bool too_many_candles_above = std::abs(m_candles_above_price_trigger) > m_config.m_candles_threshold;
    if (!too_many_candles_above && m_candles_above_price_trigger > 0 && candle.side() == SideEnum::Sell) {
        const auto signal = Signal{.side = Side::sell(), .timestamp = ts, .price = price};
        m_last_signal_side = signal.side;
        return signal;
    }
    if (!too_many_candles_above && m_candles_above_price_trigger < 0 && candle.side() == SideEnum::Buy) {
        const auto signal = Signal{.side = Side::buy(), .timestamp = ts, .price = price};
        m_last_signal_side = signal.side;
        return signal;
    }

    // Too many candles above trigger price
    m_candles_above_price_trigger = 0;
    return std::nullopt;
}

EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & CandleBollingerBandsStrategy::strategy_internal_data_channel()
{
    return m_strategy_internal_data_channel;
}
