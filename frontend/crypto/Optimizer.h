#pragma once

#include "ByBitMarketDataGateway.h"
#include "JsonStrategyConfig.h"
#include "Logger.h"

#include <nlohmann/json.hpp>

#include <utility>
#include <vector>

struct OptimizerInputs
{
    // set of configs or one config (optimize or not)
    std::variant<JsonStrategyMetaInfo, JsonStrategyConfig> entry_strategy;
    std::variant<JsonStrategyMetaInfo, JsonStrategyConfig> exit_strategy;
};

class OptimizerParser
{
public:
    OptimizerParser(OptimizerInputs optimizer_inputs)
        : m_inputs(std::move(optimizer_inputs))
    {
    }

    std::vector<std::pair<JsonStrategyConfig, JsonStrategyConfig>> get_possible_configs();

private:
    static std::vector<JsonStrategyConfig> get_possible_configs(const JsonStrategyMetaInfo & meta_info);

    const OptimizerInputs m_inputs;
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
            OptimizerInputs optimizer_data)
        : m_gateway(gateway)
        , m_symbol(std::move(symbol))
        , m_timerange(timerange)
        , m_optimizer_inputs(std::move(optimizer_data))
        , m_strategy_name(std::move(strategy_name))
        , m_exit_strategy_name(std::move(exit_strategy_name))
    {
    }

    [[nodiscard]] std::optional<std::pair<JsonStrategyConfig, JsonStrategyConfig>> optimize();

    void subscribe_for_passed_check(std::function<void(int, int)> && on_passed_checks)
    {
        if (m_on_passed_check) {
            Logger::log<LogLevel::Error>("on_passed_check already set");
        }
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
};
