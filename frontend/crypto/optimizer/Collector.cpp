#include "Collector.h"

#include "OrdinaryLeastSquares.h"

double BestProfitCriteria::operator()(const StrategyResult & result) const
{
    return result.final_profit;
}

double MinDeviationCriteria::operator()(const StrategyResult & result) const
{
    OLS::PriceRegressionFunction depo_trend{result.depo_trend_coef, result.depo_trend_const};
    if (depo_trend(result.last_position_closed_ts) <= 0.) {
        return -1.;
    }
    if (result.depo_standard_deviation <= 0.) {
        return -1.;
    }

    const double score = 1. / result.depo_standard_deviation;
    return score;
}

OptimizerCollector::OptimizerCollector(std::unique_ptr<IOptimizerCriteria> criteria)
    : m_criteria(std::move(criteria))
{
}

bool OptimizerCollector::push(
        DoubleJsonStrategyConfig strategy_config,
        const StrategyResult & result)
{
    const auto score = (*m_criteria)(result);
    if (score < 0.) {
        return false;
    }

    if (score <= m_best_score) {
        return false;
    }

    m_best_score = score;
    m_best = std::move(strategy_config);
    return true;
}
