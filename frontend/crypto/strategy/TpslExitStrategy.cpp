#include "TpslExitStrategy.h"

#include "Enums.h"
#include "EventLoop.h"
#include "Logger.h"
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
        Symbol symbol,
        JsonStrategyConfig config,
        EventLoopSubscriber<STRATEGY_EVENTS> & event_loop,
        ITradingGateway & gateway,
        EventTimeseriesChannel<double> & price_channel,
        EventObjectChannel<bool> & opened_pos_channel,
        EventTimeseriesChannel<Trade> & trades_channel)
    : ExitStrategyBase(gateway)
    , m_config(config)
    , m_symbol(std::move(symbol))
{
    m_invoker_subs.push_back(
            event_loop.invoker().register_invoker<TpslResponseEvent>([&](const auto & response) {
                handle_event(response);
            }));
    m_invoker_subs.push_back(
            event_loop.invoker().register_invoker<TpslUpdatedEvent>([&](const auto & response) {
                handle_event(response);
            }));

    m_channel_subs.push_back(price_channel.subscribe(
            event_loop.m_event_loop,
            [](auto) {},
            [this](const auto & ts, const double & price) {
                m_last_ts_and_price = {ts, price};
            }));

    m_channel_subs.push_back(opened_pos_channel.subscribe(
            event_loop.m_event_loop,
            [this](const bool & v) { m_is_pos_opened = v; }));

    m_channel_subs.push_back(trades_channel.subscribe(
            event_loop.m_event_loop,
            [](auto) {},
            [this](const auto &, const auto & trade) {
                on_trade(trade);
            }));
}

void TpslExitStrategy::on_trade(const Trade & trade)
{
    // TODO set active tpsl, not it's not set anywhere
    if (m_is_pos_opened && m_active_tpsl.has_value()) {
        // position opened and tpsl is set already
        const std::string_view msg = "TpslExitStrategy: active tpsl already exists";
        Logger::log<LogLevel::Warning>(std::string(msg));
        return;
    }

    // TODO handle a double trade case, add test
    if (m_is_pos_opened) {
        const auto tpsl = calc_tpsl(trade);
        send_tpsl(tpsl);
    }
}

void TpslExitStrategy::send_tpsl(Tpsl tpsl)
{
    TpslRequestEvent req(m_symbol, tpsl);
    m_pending_requests.emplace(req.guid);
    m_tr_gateway.push_tpsl_request(req);
}

Tpsl TpslExitStrategy::calc_tpsl(const Trade & trade)
{
    Tpsl tpsl;
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

void TpslExitStrategy::handle_event(const TpslResponseEvent & response)
{
    if (response.reject_reason.has_value()) {
        std::string err = "Rejected tpsl: " + response.reject_reason.value();
        on_error(err, true);
    }

    const size_t erased_cnt = m_pending_requests.erase(response.request_guid);
    if (erased_cnt == 0) {
        std::string err = "Unsolicited tpsl response";
        on_error(err, true);
    }

    m_tpsl_channel.push(m_last_ts_and_price.first, response.tpsl);
}

void TpslExitStrategy::handle_event(const TpslUpdatedEvent &)
{
    Logger::log<LogLevel::Debug>("TpslUpdatedEvent"); // TODO print it out
}

void TpslExitStrategy::on_error(std::string err, bool do_panic)
{
    m_error_channel.push(std::make_pair(std::move(err), do_panic));
}
