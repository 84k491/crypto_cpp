#pragma once

#include "ByBitMarketDataGateway.h"
#include "JsonStrategyConfig.h"
#include "ConfigGenerator.h"
#include "StrategyResult.h"

#include <nlohmann/json.hpp>

#include <utility>

class OptimizerCollector
{
public:
    // result -> score, negative score means drop
    using Criteria = std::function<double(const StrategyResult &)>;

    OptimizerCollector(Criteria criteria)
        : m_criteria(std::move(criteria))
    {
    }

    bool push(DoubleJsonStrategyConfig strategy_config, const StrategyResult & result);

    std::optional<DoubleJsonStrategyConfig> get_best() const { return m_best; }

private:
    Criteria m_criteria;

    double m_best_score = -1.;
    std::optional<DoubleJsonStrategyConfig> m_best;
};

class Optimizer
{
public:
    Optimizer(
            ByBitMarketDataGateway & gateway,
            Symbol symbol,
            Timerange timerange,
            std::string strategy_name,
            std::string exit_strategy_name,
            OptimizerInputs optimizer_data,
            size_t threads)
        : m_gateway(gateway)
        , m_symbol(std::move(symbol))
        , m_timerange(timerange)
        , m_optimizer_inputs(std::move(optimizer_data))
        , m_strategy_name(std::move(strategy_name))
        , m_exit_strategy_name(std::move(exit_strategy_name))
        , m_thread_count(threads)
    {
    }

    [[nodiscard]] std::optional<DoubleJsonStrategyConfig> optimize();

    void subscribe_for_passed_check(std::function<void(int, int)> && on_passed_checks)
    {
        m_on_passed_check = std::move(on_passed_checks);
    }

private:
    ByBitMarketDataGateway & m_gateway;
    Symbol m_symbol;
    Timerange m_timerange;
    OptimizerInputs m_optimizer_inputs;
    std::function<void(unsigned, unsigned)> m_on_passed_check = [](unsigned, unsigned) {};
    std::string m_strategy_name;
    std::string m_exit_strategy_name;
    size_t m_thread_count = 1;
};
