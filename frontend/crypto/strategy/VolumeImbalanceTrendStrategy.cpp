#include "VolumeImbalanceTrendStrategy.h"

#include "ScopeExit.h"

VolumeImbalanceTrendStrategyConfig::VolumeImbalanceTrendStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }

    if (json.get().contains("min_volume_imbalance_perc")) {
        m_min_volume_imbalance_perc = json.get()["min_volume_imbalance_perc"].get<int>();
    }
    if (json.get().contains("min_price_change_perc")) {
        m_min_price_change_perc = json.get()["min_price_change_perc"].get<int>();
    }
    if (json.get().contains("min_trade_rate_threshold_perc")) {
        m_min_trade_rate_threshold_perc = json.get()["min_trade_rate_threshold_perc"].get<int>();
    }
    if (json.get().contains("max_relative_volatility_perc")) {
        m_max_relative_volatility_perc = json.get()["max_relative_volatility_perc"].get<double>();
    }
    if (json.get().contains("lookback_period")) {
        const int period_in_candles = json.get()["lookback_period"].get<int>();
        m_lookback_period = m_timeframe * period_in_candles;
    }

    if (json.get().contains("risk")) {
        m_risk = json.get()["risk"].get<double>();
    }
    if (json.get().contains("no_loss_coef")) {
        m_no_loss_coef = json.get()["no_loss_coef"].get<double>();
    }
}

bool VolumeImbalanceTrendStrategyConfig::is_valid() const
{
    return m_min_volume_imbalance_perc <= 100 && m_min_price_change_perc <= 100;
}

JsonStrategyConfig VolumeImbalanceTrendStrategyConfig::to_json() const
{
    using namespace std::chrono;

    nlohmann::json json;
    json["timeframe_s"] = duration_cast<seconds>(m_timeframe).count();
    json["min_volume_imbalance_perc"] = m_min_volume_imbalance_perc;
    json["min_price_change_perc"] = m_min_price_change_perc;
    json["min_trade_rate_threshold_perc"] = m_min_trade_rate_threshold_perc;
    json["max_relative_volatility_perc"] = m_max_relative_volatility_perc;
    json["lookback_period"] = duration_cast<seconds>(m_lookback_period).count() / duration_cast<seconds>(m_timeframe).count();
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

JsonStrategyConfig VolumeImbalanceTrendStrategyConfig::make_exit_strategy_config() const
{
    nlohmann::json json;
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

VolumeImbalanceTrendStrategy::VolumeImbalanceTrendStrategy(
        const VolumeImbalanceTrendStrategyConfig & config,
        EventLoop & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_config(config)
    , m_exit_strategy(
              orders,
              m_config.make_exit_strategy_config(),
              event_loop,
              channels)
    , m_trade_rate_avg(m_config.m_lookback_period)
    , m_std(m_config.m_lookback_period)
    , m_sub(event_loop)
{
    m_sub.subscribe(
            channels.candle_channel,
            [](const auto &) {},
            [this](const auto &, const Candle & candle) {
                push_candle(candle);
            });
}

bool VolumeImbalanceTrendStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> VolumeImbalanceTrendStrategy::timeframe() const
{
    return m_config.m_timeframe;
}

void VolumeImbalanceTrendStrategy::push_candle(const Candle & candle)
{
    const auto trade_rate = candle.trade_count();

    const auto trade_rate_avg_opt = m_trade_rate_avg.push_value({candle.close_ts(), trade_rate});
    ScopeExit se1{[&] { m_prev_trade_rate_avg_opt = trade_rate_avg_opt; }};

    const auto volatility = m_std.push_value(candle.close_ts(), candle.close());
    const auto rel_volatility_opt = [&] -> decltype(volatility) {
        if (!volatility) {
            return std::nullopt;
        }
        return volatility.value() / m_std.mean();
    }();

    const auto imbalance = candle.volume_imbalance_coef();
    const auto price_diff_perc = candle.price_diff_coef() * 100;

    m_strategy_internal_data_channel.push(candle.close_ts(), {"trade_rate", "trade_rate", static_cast<double>(trade_rate)});

    m_strategy_internal_data_channel.push(candle.close_ts(), {"price_diff", "price_diff", price_diff_perc});
    m_strategy_internal_data_channel.push(candle.close_ts(), {"price_diff", "lower_limit", static_cast<double>(m_config.m_min_price_change_perc)});

    m_strategy_internal_data_channel.push(candle.close_ts(), {"volume_imbalance", "volume_imbalance", imbalance.unsigned_coef * 100});
    m_strategy_internal_data_channel.push(candle.close_ts(), {"volume_imbalance", "lower_limit", static_cast<double>(m_config.m_min_volume_imbalance_perc)});

    {
        if (!rel_volatility_opt.has_value()) {
            return;
        }
        if (!trade_rate_avg_opt || !m_prev_trade_rate_avg_opt) {
            return;
        }
        if (!rel_volatility_opt) {
            return;
        }
    }

    const double trade_rate_lower_limit = *m_prev_trade_rate_avg_opt * (double)m_config.m_min_trade_rate_threshold_perc / 100;

    m_strategy_internal_data_channel.push(candle.close_ts(), {"volatitily", "volatitily", *rel_volatility_opt * 100});
    m_strategy_internal_data_channel.push(candle.close_ts(), {"volatitily", "upper_limit", m_config.m_max_relative_volatility_perc});

    m_strategy_internal_data_channel.push(candle.close_ts(), {"trade_rate", "trade_rate_avg", *trade_rate_avg_opt});
    m_strategy_internal_data_channel.push(candle.close_ts(), {"trade_rate", "lower_limit", trade_rate_lower_limit});

    {
        const bool volatility_ok = *rel_volatility_opt * 100 < m_config.m_max_relative_volatility_perc;
        const bool trade_rate_ok = trade_rate > trade_rate_lower_limit;
        const bool imbalance_triggered = imbalance.unsigned_coef * 100 > m_config.m_min_volume_imbalance_perc;
        const bool price_diff_ok = price_diff_perc > m_config.m_min_price_change_perc;
        const bool imbalance_and_price_sides_match = (imbalance.side.sign() == 1) == (price_diff_perc >= 0);

        if (!volatility_ok ||
            !trade_rate_ok ||
            !imbalance_triggered ||
            !price_diff_ok ||
            !imbalance_and_price_sides_match) {
            return;
        }
    }

    try_send_order(imbalance.side, candle.close(), candle.close_ts());
}
