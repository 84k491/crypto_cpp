#pragma once

#include "JsonStrategyConfig.h"
#include "StrategyResult.h"

class IOptimizerCriteria
{
public:
    virtual ~IOptimizerCriteria() = default;

    // result -> score, negative score means drop
    virtual double operator()(const StrategyResult & result) const = 0;
};

class BestProfitCriteria : public IOptimizerCriteria
{
public:
    double operator()(const StrategyResult & result) const override;
};

class MinDeviationCriteria : public IOptimizerCriteria
{
public:
    double operator()(const StrategyResult & result) const override;
};

class OptimizerCollector
{
public:
    OptimizerCollector(std::unique_ptr<IOptimizerCriteria> criteria);

    bool push(DoubleJsonStrategyConfig strategy_config, const StrategyResult & result);

    std::optional<DoubleJsonStrategyConfig> get_best() const { return m_best; }

private:
    std::unique_ptr<IOptimizerCriteria> m_criteria;

    double m_best_score = -1.;
    std::optional<DoubleJsonStrategyConfig> m_best;
};
