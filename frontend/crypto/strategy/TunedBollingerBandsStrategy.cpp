#include "TunedBollingerBandsStrategy.h"

#include "ScopeExit.h"

#include <print>

TunedBollingerBandsStrategyConfig::TunedBollingerBandsStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("std_dev")) {
        m_std_deviation_coefficient = json.get()["std_dev"].get<double>();
    }
    if (json.get().contains("interval_m")) {
        m_interval = std::chrono::minutes(json.get()["interval_m"].get<int>());
    }
}

bool TunedBollingerBandsStrategyConfig::is_valid() const
{
    return m_interval > std::chrono::minutes(0) && m_std_deviation_coefficient > 0.;
}

JsonStrategyConfig TunedBollingerBandsStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["std_dev"] = m_std_deviation_coefficient;
    json["interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_interval).count();
    return json;
}

TunedBollingerBandsStrategy::TunedBollingerBandsStrategy(const TunedBollingerBandsStrategyConfig & config)
    : m_config(config)
    , m_price_filter(price_filter_interval)
    , m_bollinger_bands(config.m_interval, config.m_std_deviation_coefficient)
{
}

bool TunedBollingerBandsStrategy::is_valid() const
{
    return m_config.is_valid();
}

#define UNWRAP_RET(var, ret_value) \
    if (!var##_opt.has_value()) {  \
        return ret_value;          \
    }                              \
    const auto &(var) = var##_opt.value();

std::optional<Signal> TunedBollingerBandsStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    const auto bb_res_opt = m_bollinger_bands.push_value(ts_and_price);
    UNWRAP_RET(bb_res, std::nullopt);

    const auto filtered_price_opt = m_price_filter.push_value(ts_and_price);
    UNWRAP_RET(filtered_price, std::nullopt);

    ScopeExit se([filtered_price, this] { m_last_filtered_price_opt = filtered_price; });
    UNWRAP_RET(m_last_filtered_price, std::nullopt);

    const auto & [ts, input_price] = ts_and_price;
    m_strategy_internal_data_channel.push(ts, {"prices", "upper_band", bb_res.m_upper_band});
    m_strategy_internal_data_channel.push(ts, {"prices", "trend", bb_res.m_trend});
    m_strategy_internal_data_channel.push(ts, {"prices", "lower_band", bb_res.m_lower_band});
    m_strategy_internal_data_channel.push(ts, {"prices", "filtered_price", filtered_price});

    if (m_last_signal_side) {
        const auto last_signal_side = m_last_signal_side.value();
        switch (last_signal_side.value()) {
        case SideEnum::Buy:
            if (input_price > bb_res.m_trend) {
                m_last_signal_side = {};
            }
            break;
        case SideEnum::Sell: {
            if (input_price < bb_res.m_trend) {
                m_last_signal_side = {};
            }
            break;
        }
        }
        return std::nullopt;
    }

    if (filtered_price > bb_res.m_upper_band &&
        input_price > bb_res.m_upper_band &&
        m_last_filtered_price >= filtered_price) {
        const auto signal = Signal{.side = Side::sell(), .timestamp = ts, .price = ts_and_price.second};
        m_last_signal_side = signal.side;
        return signal;
    }
    if (filtered_price < bb_res.m_lower_band &&
        input_price < bb_res.m_lower_band &&
        m_last_filtered_price <= filtered_price) {
        const auto signal = Signal{.side = Side::buy(), .timestamp = ts, .price = ts_and_price.second};
        m_last_signal_side = signal.side;
        return signal;
    }
    return std::nullopt;
}

EventTimeseriesChannel<std::tuple<std::string, std::string, double>> & TunedBollingerBandsStrategy::strategy_internal_data_channel()
{
    return m_strategy_internal_data_channel;
}
