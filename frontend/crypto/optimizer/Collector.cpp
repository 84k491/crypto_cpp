#include "Collector.h"

#include "Logger.h"
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

OptimizerCollector::OptimizerCollector(std::string criteria_name, const std::vector<FilterParams> & filters)
{
#define TRY_BUILD_CRITERIA(name)                         \
    if (criteria_name == #name) {                        \
        m_criteria = std::make_unique<name##Criteria>(); \
    }

    TRY_BUILD_CRITERIA(BestProfit);
    TRY_BUILD_CRITERIA(MinDeviation);

    if (!m_criteria) {
        Logger::logf<LogLevel::Error>("Unknown criteria {}", criteria_name);
    }

#undef TRY_BUILD_CRITERIA

#define TRY_BUILD_FILTER(name)                                              \
    if (filter.filter_name == #name) {                                      \
        m_filters.add_filter(std::make_unique<name##Filter>(filter.value)); \
    }

    for (const auto & filter : filters) {
        TRY_BUILD_FILTER(Apr);
    }

#undef TRY_BUILD_FILTER
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

bool ConjunctiveFilterAggregation::operator()(const StrategyResult & result) const
{
    return std::ranges::all_of(m_filters, [&](const auto & filter) { return (*filter)(result); });
}
