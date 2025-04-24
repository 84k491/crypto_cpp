#pragma once

#include "JsonStrategyConfig.h"

#include <set>

struct StrategyOptimizerInputs
{
    JsonStrategyMetaInfo meta;
    JsonStrategyConfig current_config;
    std::set<std::string> optimizable_parameters;
};

struct OptimizerInputs
{
    StrategyOptimizerInputs entry_strategy;
    StrategyOptimizerInputs exit_strategy;
};

// TODO implement tests
class OptimizerParser
{
public:
    OptimizerParser(OptimizerInputs optimizer_inputs)
        : m_inputs(std::move(optimizer_inputs))
    {
    }

    std::vector<DoubleJsonStrategyConfig> get_possible_configs();

private:
    static std::vector<JsonStrategyConfig> get_possible_configs(const StrategyOptimizerInputs & strategy_optimizer_inputs);

    const OptimizerInputs m_inputs;
};

