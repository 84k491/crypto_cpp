#pragma once

#include "JsonStrategyConfig.h"
#include "Tpsl.h"

class Trade;

class TpslExitStrategyConfig
{
    friend std::ostream & operator<<(std::ostream & os, const TpslExitStrategyConfig & config);
public:
    TpslExitStrategyConfig(const JsonStrategyConfig & json);
    TpslExitStrategyConfig(double risk, double risk_reward_ratio);

    JsonStrategyConfig to_json() const;

    bool is_valid() const;

    auto risk() const { return m_risk; }
    auto risk_reward_ratio() const { return m_risk_reward_ratio; }

private:
    double m_risk = 1.;
    double m_risk_reward_ratio = 1.;
};

class TpslExitStrategy
{
public:
    TpslExitStrategy(TpslExitStrategyConfig config);

    Tpsl calc_tpsl(const Trade & trade);

private:
    TpslExitStrategyConfig m_config;
};
