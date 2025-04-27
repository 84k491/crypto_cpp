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

class IOptimizerFilter
{
public:
    virtual ~IOptimizerFilter() = default;

    virtual bool operator()(const StrategyResult & result) const = 0;
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

class AprFilter : public IOptimizerFilter
{
public:
    AprFilter(double min_apr)
        : m_min_apr(min_apr)
    {
    }

    bool operator()(const StrategyResult & result) const override
    {
        return result.apr() >= m_min_apr;
    }

private:
    double m_min_apr = .0;
};

class ConjunctiveFilterAggregation
{
public:
    void add_filter(std::unique_ptr<IOptimizerFilter> filter) { m_filters.push_back(std::move(filter)); }

    bool operator()(const StrategyResult & result) const;

private:
    std::vector<std::unique_ptr<IOptimizerFilter>> m_filters;
};

class OptimizerCollector
{
public:
    struct FilterParams
    {
        std::string filter_name;
        double value;
    };

    OptimizerCollector(std::string criteria_name, const std::vector<FilterParams> & filters);

    bool push(DoubleJsonStrategyConfig strategy_config, const StrategyResult & result);

    std::optional<DoubleJsonStrategyConfig> get_best() const { return m_best; }

private:
    std::unique_ptr<IOptimizerCriteria> m_criteria;

    ConjunctiveFilterAggregation m_filters;

    double m_best_score = -1.;
    std::optional<DoubleJsonStrategyConfig> m_best;
};
