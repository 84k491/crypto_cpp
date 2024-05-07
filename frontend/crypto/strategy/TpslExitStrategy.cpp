#include "TpslExitStrategy.h"

#include "Enums.h"
#include "Trade.h"

#include <iostream>

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

    // std::cout << "Calculated from price " << entry_price << ", side: " << (trade.side == Side::Buy ? "Buy" : "Sell") << " Tpsl: " << tpsl << std::endl;
    return tpsl;
}

bool TpslExitStrategyConfig::is_valid() const
{
    bool limits_ok = m_risk > 0. && m_risk < 1. && m_risk_reward_ratio > 0. && m_risk_reward_ratio < 1.;
    return limits_ok;
}

TpslExitStrategy::TpslExitStrategy(TpslExitStrategyConfig config)
    : m_config(config)
{
}
