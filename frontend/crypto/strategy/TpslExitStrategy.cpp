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

Tpsl TpslExitStrategy::calc_tpsl(const Trade & trade)
{
    // TODO calc fees
    Tpsl tpsl;
    const double & entry_price = trade.price;

    double risk = entry_price * m_config.risk();
    double profit = entry_price * (m_config.risk() / m_config.risk_reward_ratio());

    switch (trade.side) {
    case Side::Buy: {
        tpsl.take_profit_price = entry_price + profit;
        tpsl.stop_loss_price = entry_price - risk;
        break;
    }
    case Side::Sell: {
        tpsl.stop_loss_price = entry_price + risk;
        tpsl.take_profit_price = entry_price - profit;
        break;
    }
    case Side::Close: {
    }
    }

    std::cout << "Calculated from price " << entry_price << ", side: " << (trade.side == Side::Buy ? "Buy" : "Sell") << " Tpsl: " << tpsl << std::endl;
    return tpsl;
}

bool TpslExitStrategyConfig::is_valid() const
{
    return m_risk > 0. && m_risk < 1. && m_risk_reward_ratio > 0. && m_risk_reward_ratio < 1.;
}

TpslExitStrategy::TpslExitStrategy(TpslExitStrategyConfig config)
    : m_config(config)
{
}
