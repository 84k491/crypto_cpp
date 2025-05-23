#include "BollingerBandsStrategy.h"

#include "Enums.h"
#include "EventLoopSubscriber.h"

BollingerBandsStrategyConfig::BollingerBandsStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("std_dev")) {
        m_std_deviation_coefficient = json.get()["std_dev"].get<double>();
    }
    if (json.get().contains("interval_m")) {
        m_interval = std::chrono::minutes(json.get()["interval_m"].get<int>());
    }
}

bool BollingerBandsStrategyConfig::is_valid() const
{
    return m_interval > std::chrono::minutes(0) && m_std_deviation_coefficient > 0.;
}

JsonStrategyConfig BollingerBandsStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["std_dev"] = m_std_deviation_coefficient;
    json["interval_m"] = std::chrono::duration_cast<std::chrono::minutes>(m_interval).count();
    return json;
}

BollingerBandsStrategy::BollingerBandsStrategy(
        const BollingerBandsStrategyConfig & config,
        EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
        EventTimeseriesChannel<double> & price_channel)
    : m_config(config)
    , m_bollinger_bands(config.m_interval, config.m_std_deviation_coefficient)
{
    m_channel_subs.push_back(price_channel.subscribe(
            event_loop.m_event_loop,
            [](auto) {},
            [this](const auto & ts, const double & price) {
                if (const auto signal_opt = push_price({ts, price}); signal_opt) {
                    m_signal_channel.push(ts, signal_opt.value());
                }
            }));
}

bool BollingerBandsStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> BollingerBandsStrategy::timeframe() const
{
    return {};
}

std::optional<Signal> BollingerBandsStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    const auto bb_res_opt = m_bollinger_bands.push_value(ts_and_price);
    if (!bb_res_opt.has_value()) {
        return std::nullopt;
    }
    const auto & bb_res = bb_res_opt.value();
    const auto & [ts, price] = ts_and_price;
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

    if (price > bb_res.m_upper_band) {
        const auto signal = Signal{.side = Side::sell(), .timestamp = ts, .price = ts_and_price.second};
        m_last_signal_side = signal.side;
        return signal;
    }
    if (price < bb_res.m_lower_band) {
        const auto signal = Signal{.side = Side::buy(), .timestamp = ts, .price = ts_and_price.second};
        m_last_signal_side = signal.side;
        return signal;
    }

    return std::nullopt;
}
