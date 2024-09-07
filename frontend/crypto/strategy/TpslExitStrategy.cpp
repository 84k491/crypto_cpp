#include "TpslExitStrategy.h"

#include "Enums.h"
#include "Logger.h"
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

TpslExitStrategy::TpslExitStrategy(Symbol symbol,
                                   const JsonStrategyConfig & config,
                                   std::shared_ptr<EventLoop<STRATEGY_EVENTS>> & event_loop,
                                   ITradingGateway & gateway)
    : ExitStrategyBase(gateway)
    , m_config(config)
    , m_event_loop(event_loop)
    , m_symbol(std::move(symbol))
{
}

std::optional<std::string> TpslExitStrategy::on_price_changed(std::pair<std::chrono::milliseconds, double> ts_and_price)
{
    m_last_ts_and_price = std::move(ts_and_price);
    return std::nullopt;
}

std::optional<std::string> TpslExitStrategy::on_trade(const std::optional<OpenedPosition> & opened_position, const Trade & trade)
{
    // TODO set active tpsl
    if (m_opened_position.has_value() && m_active_tpsl.has_value()) {
        const std::string_view msg = "TpslExitStrategy: active tpsl already exists";
        Logger::log<LogLevel::Warning>(std::string(msg));
        return std::string(msg);
    }

    m_opened_position = opened_position;

    // TODO handle a double trade case, add test
    if (opened_position) {
        const auto tpsl = calc_tpsl(trade);
        send_tpsl(tpsl);
    }
    return std::nullopt;
}

void TpslExitStrategy::send_tpsl(Tpsl tpsl)
{
    TpslRequestEvent req(m_symbol, tpsl, m_event_loop);
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

    switch (trade.side()) {
    case Side::Buy: {
        tpsl.take_profit_price = entry_price + profit + fee_price_delta;
        tpsl.stop_loss_price = entry_price - risk + fee_price_delta;
        break;
    }
    case Side::Sell: {
        tpsl.stop_loss_price = entry_price + risk - fee_price_delta;
        tpsl.take_profit_price = entry_price - profit - fee_price_delta;
        break;
    }
    case Side::Close: {
    }
    }

    return tpsl;
}

bool TpslExitStrategyConfig::is_valid() const
{
    bool limits_ok = m_risk > 0. && m_risk < 1. && m_risk_reward_ratio > 0. && m_risk_reward_ratio < 1.;
    return limits_ok;
}

std::optional<std::pair<std::string, bool>> TpslExitStrategy::handle_event(const TpslResponseEvent & response)
{
    if (response.reject_reason.has_value()) {
        std::string err = "Rejected tpsl: " + response.reject_reason.value();
        return {{err, true}};
    }

    const size_t erased_cnt = m_pending_requests.erase(response.request_guid);
    if (erased_cnt == 0) {
        std::string err = "Unsolicited tpsl response";
        return {{err, false}};
    }

    m_tpsl_publisher.push(m_last_ts_and_price.first, response.tpsl);
    return std::nullopt;
}

[[nodiscard]] std::optional<std::pair<std::string, bool>>
TpslExitStrategy::handle_event(const TpslUpdatedEvent &)
{
    Logger::log<LogLevel::Debug>("TpslUpdatedEvent"); // TODO print it out
    return std::nullopt;
}

std::optional<std::pair<std::string, bool>>
TpslExitStrategy::handle_event(const TrailingStopLossResponseEvent &)
{
    return {{"Trailing stop in tpsl", true}};
}

std::optional<std::pair<std::string, bool>>
TpslExitStrategy::handle_event(const TrailingStopLossUpdatedEvent &)
{
    return {{"Trailing stop in tpsl", true}};
}
