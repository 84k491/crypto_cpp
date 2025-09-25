#include "BollingerBandsStrategy.h"

#include "Enums.h"
#include "EventLoopSubscriber.h"
#include "StrategyBase.h"

BollingerBandsStrategyConfig::BollingerBandsStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("std_dev")) {
        m_std_deviation_coefficient = json.get()["std_dev"].get<double>();
    }
    if (json.get().contains("interval_m")) {
        m_interval = std::chrono::minutes(json.get()["interval_m"].get<int>());
    }
    if (json.get().contains("risk")) {
        m_risk = json.get()["risk"].get<double>();
    }
    if (json.get().contains("no_loss_coef")) {
        m_no_loss_coef = json.get()["no_loss_coef"].get<double>();
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
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

JsonStrategyConfig BollingerBandsStrategyConfig::make_exit_strategy_config() const
{
    nlohmann::json json;
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

BollingerBandsStrategy::BollingerBandsStrategy(
        const BollingerBandsStrategyConfig & config,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_config(config)
    , m_exit_strategy(orders,
                      config.make_exit_strategy_config(),
                      event_loop,
                      channels)
    , m_bollinger_bands(config.m_interval, config.m_std_deviation_coefficient)
{
    event_loop.subscribe(
            channels.price_channel,
            [](const auto &) {},
            [this](const auto & ts, const double & price) {
                push_price({ts, price});
            });
}

bool BollingerBandsStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> BollingerBandsStrategy::timeframe() const
{
    return {};
}

void BollingerBandsStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    const auto bb_res_opt = m_bollinger_bands.push_value(ts_and_price);
    if (!bb_res_opt.has_value()) {
        return;
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
        return;
    }

    if (price > bb_res.m_upper_band) {
        try_send_order(Side::sell(), ts_and_price.second, ts);
        m_last_signal_side = Side::sell();
    }

    if (price < bb_res.m_lower_band) {
        try_send_order(Side::buy(), ts_and_price.second, ts);
        m_last_signal_side = Side::buy();
    }
}
