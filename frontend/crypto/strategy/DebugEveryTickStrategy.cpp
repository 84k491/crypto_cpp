#include "DebugEveryTickStrategy.h"

#include "EventLoopSubscriber.h"
#include "StrategyBase.h"

DebugEveryTickStrategyConfig::DebugEveryTickStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("risk")) {
        m_risk = json.get()["risk"].get<double>();
    }
    if (json.get().contains("no_loss_coef")) {
        m_no_loss_coef = json.get()["no_loss_coef"].get<double>();
    }
}

bool DebugEveryTickStrategyConfig::is_valid()
{
    return true;
}

JsonStrategyConfig DebugEveryTickStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

JsonStrategyConfig DebugEveryTickStrategyConfig::make_exit_strategy_config() const
{
    nlohmann::json json;
    json["risk"] = m_risk;
    json["no_loss_coef"] = m_no_loss_coef;
    return json;
}

DebugEveryTickStrategy::DebugEveryTickStrategy(
        const DebugEveryTickStrategyConfig & conf,
        EventLoopSubscriber & event_loop,
        StrategyChannelsRefs channels,
        OrderManager & orders)
    : StrategyBase(orders, event_loop, channels)
    , m_config(conf)
    , m_exit_strategy(orders,
                      conf.make_exit_strategy_config(),
                      event_loop,
                      channels)
{
    m_channel_subs.push_back(channels.price_channel.subscribe(
            event_loop.m_event_loop,
            [](const auto &) {},
            [this](const auto & ts, const double & price) {
                push_price({ts, price});
            }));
}

void DebugEveryTickStrategy::push_price(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    iteration = (iteration + 1) % max_iter;
    if (iteration != 0) {
        return;
    }

    const auto side = last_side.opposite();
    last_side = side;

    try_send_order(side, ts_and_price.second, ts_and_price.first);
}

bool DebugEveryTickStrategy::is_valid() const
{
    return DebugEveryTickStrategyConfig::is_valid();
}
