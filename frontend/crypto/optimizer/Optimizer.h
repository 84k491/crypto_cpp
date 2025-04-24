#pragma once

#include "ByBitMarketDataGateway.h"
#include "JsonStrategyConfig.h"
#include "ConfigGenerator.h"

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
            size_t threads);

    [[nodiscard]] std::optional<DoubleJsonStrategyConfig> optimize();

    void subscribe_for_passed_check(std::function<void(int, int)> && on_passed_checks);

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
