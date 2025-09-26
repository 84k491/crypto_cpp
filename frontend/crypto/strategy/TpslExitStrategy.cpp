#include "TpslExitStrategy.h"

#include "Enums.h"
#include "EventLoop.h"
#include "Logger.h"
#include "OrderManager.h"
#include "Side.h"
#include "Trade.h"

#include <iostream>
#include <utility>

TpslExitStrategyConfig::TpslExitStrategyConfig(const JsonStrategyConfig & json)
{
    if (json.get().contains("risk")) {
        m_risk = json.get()["risk"].get<double>();
    }
    if (json.get().contains("risk_reward_ratio")) {
        m_risk_reward_ratio = json.get()["risk_reward_ratio"].get<double>();
    }
}

JsonStrategyConfig TpslExitStrategyConfig::to_json() const
{
    nlohmann::json json;
    json["risk"] = m_risk;
    json["risk_reward_ratio"] = m_risk_reward_ratio;
    return json;
}

std::ostream & operator<<(std::ostream & os, const TpslExitStrategyConfig & config)
{
    os << "risk = " << config.risk() << ", risk_reward_ratio = " << config.risk_reward_ratio();
    return os;
}

TpslExitStrategyConfig::TpslExitStrategyConfig(double risk, double risk_reward_ratio)
    : m_risk(risk)
    , m_risk_reward_ratio(risk_reward_ratio)
{
}

TpslExitStrategy::TpslExitStrategy(
        OrderManager & orders,
        JsonStrategyConfig config,
        std::shared_ptr<EventLoop> & event_loop,
        StrategyChannelsRefs channels)
    : m_orders(orders)
    , m_config(config)
    , m_event_loop{event_loop}
    , m_main_sub{event_loop}
{
    m_main_sub.subscribe(
            channels.price_channel,
            [](const auto &) {},
            [this](const auto & ts, const double & price) {
                m_last_ts_and_price = {ts, price};
            });

    m_main_sub.subscribe(
            channels.opened_pos_channel,
            [this](const bool & v) {
                m_is_pos_opened = v;
            });

    m_main_sub.subscribe(
            channels.trades_channel,
            [](const auto &) {},
            [this](const auto &, const auto & trade) {
                on_trade(trade);
            });
}

void TpslExitStrategy::on_trade(const Trade & trade)
{
    // TODO set active tpsl, not it's not set anywhere
    if (m_is_pos_opened && m_active_tpsl != nullptr) {
        // position opened and tpsl is set already
        const std::string_view msg = "TpslExitStrategy: active tpsl already exists";
        Logger::log<LogLevel::Warning>(std::string(msg));
        return;
    }

    // TODO handle a double trade case, add test
    if (m_is_pos_opened) {
        const auto tpsl = calc_tpsl(trade);
        auto & ch = m_orders.send_tpsl(
                tpsl.take_profit_price,
                tpsl.stop_loss_price,
                trade.side().opposite(),
                trade.ts());
        m_tpsl_sub = EventSubcriber{m_event_loop};
        m_tpsl_sub->subscribe(
                ch,
                [&](const std::shared_ptr<TpslFullPos> & sptr) {
                    on_updated(sptr);
                });
    }
}

void TpslExitStrategy::on_updated(const std::shared_ptr<TpslFullPos> & sptr)
{
    m_active_tpsl = sptr;

    if (!m_active_tpsl) {
        return;
    }

    if (m_active_tpsl->status() == OrderStatus::Rejected) {
        on_error(m_active_tpsl->m_reject_reason.value_or(""), true);
    }

    if (m_active_tpsl->status() == OrderStatus::Filled ||
        m_active_tpsl->status() == OrderStatus::Cancelled) {

        m_active_tpsl.reset();
        m_tpsl_sub.reset();
    }
}

TpslFullPos::Prices TpslExitStrategy::calc_tpsl(const Trade & trade)
{
    TpslFullPos::Prices tpsl;
    const double & entry_price = trade.price();
    const double total_fee = trade.fee() * 2;
    const double fee_price_delta = total_fee / trade.unsigned_volume().value();

    double risk = entry_price * m_config.risk();
    double profit = entry_price * (m_config.risk() / m_config.risk_reward_ratio());

    switch (trade.side().value()) {
    case SideEnum::Buy: {
        tpsl.take_profit_price = entry_price + profit + fee_price_delta;
        tpsl.stop_loss_price = entry_price - risk + fee_price_delta;
        break;
    }
    case SideEnum::Sell: {
        tpsl.stop_loss_price = entry_price + risk - fee_price_delta;
        tpsl.take_profit_price = entry_price - profit - fee_price_delta;
        break;
    }
    }

    return tpsl;
}

bool TpslExitStrategyConfig::is_valid() const
{
    bool limits_ok = m_risk > 0. && m_risk < 1. && m_risk_reward_ratio > 0. && m_risk_reward_ratio < 1.;
    return limits_ok;
}

void TpslExitStrategy::on_error(std::string err, bool do_panic)
{
    m_error_channel.push(std::make_pair(std::move(err), do_panic));
}
