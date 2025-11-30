#include "TrendCatcherStrategy.h"

#include "Logger.h"

namespace {

int sign(double v)
{
    if (v < 0) {
        return -1;
    }

    if (v > 0) {
        return 1;
    }

    return 0;
}

} // namespace

TrendCatcherStrategyConfig::TrendCatcherStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("timeframe_s")) {
        m_timeframe = std::chrono::seconds(json.get()["timeframe_s"].get<int>());
    }

    if (json.get().contains("fast_interval")) {
        auto interval = json.get()["fast_interval"].get<int>();
        m_fast_interval = m_timeframe * interval;
    }

    if (json.get().contains("slow_interval")) {
        auto interval = json.get()["slow_interval"].get<int>();
        m_slow_interval = m_timeframe * interval;
    }

    if (json.get().contains("min_price_diff_perc")) {
        m_min_price_diff_perc = json.get()["min_price_diff_perc"].get<double>();
    }

    if (json.get().contains("max_std_dev_perc")) {
        m_max_std_dev_perc = json.get()["max_std_dev_perc"].get<double>();
    }

    // exit
    {
        if (json.get().contains("risk")) {
            m_risk = json.get()["risk"].get<double>();
        }

        if (json.get().contains("risk_reward_ratio")) {
            m_risk_reward_ratio = json.get()["risk_reward_ratio"].get<double>();
        }
    }
}

bool TrendCatcherStrategyConfig::is_valid() const
{
    return true;
}

JsonStrategyConfig TrendCatcherStrategyConfig::to_json() const
{
    nlohmann::json json;

    int timeframe_s = std::chrono::duration_cast<std::chrono::seconds>(m_timeframe).count();
    json["timeframe_s"] = timeframe_s;

    json["slow_interval"] = m_slow_interval.count() / m_timeframe.count();
    json["fast_interval"] = m_fast_interval.count() / m_timeframe.count();

    json["min_price_diff_perc"] = m_min_price_diff_perc;
    json["max_std_dev_perc"] = m_max_std_dev_perc;

    // exit

    json["risk"] = m_risk;
    json["risk_reward_ratio"] = m_risk_reward_ratio;

    return json;
}

JsonStrategyConfig TrendCatcherStrategyConfig::make_exit_strategy_config() const
{
    nlohmann::json json;

    json["risk"] = m_risk;
    json["risk_reward_ratio"] = m_risk_reward_ratio;

    return json;
}

bool TrendCatcherStrategy::is_valid() const
{
    return m_config.is_valid();
}

std::optional<std::chrono::milliseconds> TrendCatcherStrategy::timeframe() const
{
    return m_config.m_timeframe;
}

TrendCatcherStrategy::TrendCatcherStrategy(
        const TrendCatcherStrategyConfig & config,
        EventLoop & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_config(config)
    , m_slow_diff_ma(m_config.m_slow_interval)
    , m_slow_std_dev(m_config.m_slow_interval)
    , m_fast_diff_ma(m_config.m_fast_interval)
    , m_fast_std_dev(m_config.m_fast_interval)
    , m_exit(orders, config.make_exit_strategy_config(), event_loop, channels)
    , m_sub(event_loop)
{
    m_sub.subscribe(
            channels.candle_channel,
            [](const auto &) {},
            [this](const auto &, const Candle & candle) {
                push_candle(candle);
            });
}

void TrendCatcherStrategy::push_candle(const Candle & candle)
{
    const auto ts = candle.close_ts();

    const auto slow_diff_opt = m_slow_diff_ma.push_value({ts, candle.price_diff_coef()});
    const auto fast_diff_opt = m_fast_diff_ma.push_value({ts, candle.price_diff_coef()});

    const auto slow_std_dev_opt = m_slow_std_dev.push_value(ts, candle.price_diff_coef());
    const auto fast_std_dev_opt = m_fast_std_dev.push_value(ts, candle.price_diff_coef());

    // LOG_DEBUG("Slow interval sec: {}", std::chrono::duration_cast<std::chrono::seconds>(m_config.m_slow_interval));

    if (!slow_diff_opt.has_value() || !fast_diff_opt.has_value() || !slow_std_dev_opt.has_value() || !fast_std_dev_opt.has_value()) {
        return;
    }

    // LOG_DEBUG("got values");

    // p for percent
    double slow_diff_p = *slow_diff_opt * 100.;
    double fast_diff_p = *fast_diff_opt * 100.;
    double slow_std_dev_p = *slow_std_dev_opt * 100.;
    double fast_std_dev_p = *fast_std_dev_opt * 100.;

    m_strategy_internal_data_channel.push(ts, {.chart_name = "price_diff", .series_name = "slow_diff,%", .value = slow_diff_p});
    m_strategy_internal_data_channel.push(ts, {.chart_name = "price_diff", .series_name = "fast_diff,%", .value = fast_diff_p});

    m_strategy_internal_data_channel.push(ts, {.chart_name = "std_dev", .series_name = "slow_std_dev,%", .value = slow_std_dev_p});
    m_strategy_internal_data_channel.push(ts, {.chart_name = "std_dev", .series_name = "fast_std_dev,%", .value = fast_std_dev_p});

    if (sign(slow_diff_p) != sign(fast_diff_p)) {
        return;
    }
    if ((sign(slow_diff_p) == 0) || (sign(fast_diff_p) == 0)) {
        return;
    }

    // TODO convert from percents to coef
    if (std::fabs(slow_diff_p) < m_config.m_min_price_diff_perc || std::fabs(fast_diff_p) < m_config.m_min_price_diff_perc) {
        return;
    }

    // TODO convert from percents to coef
    if (slow_std_dev_p > m_config.m_max_std_dev_perc || fast_std_dev_p > m_config.m_max_std_dev_perc) {
        return;
    }

    const auto order_side = sign(fast_diff_p) > 0 ? Side::buy() : Side::sell();
    try_send_order(order_side, candle.close(), ts);
}
