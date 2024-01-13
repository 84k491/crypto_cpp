#pragma once

#include "ByBitGateway.h"
#include "JsonStrategyConfig.h"

#include <nlohmann/json.hpp>

#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

class OptimizerParser
{
public:
    OptimizerParser(JsonStrategyMetaInfo optimizer_data)
        : m_data(std::move(optimizer_data))
    {
    }

    std::vector<nlohmann::json> get_possible_configs();
    std::string get_strategy_name() const;

private:
    const JsonStrategyMetaInfo m_data;
};

class Optimizer
{
public:
    Optimizer(ByBitGateway & gateway, Symbol symbol, Timerange timerange, JsonStrategyMetaInfo optimizer_data)
        : m_gateway(gateway)
        , m_symbol(std::move(symbol))
        , m_timerange(std::move(timerange))
        , m_optimizer_data(std::move(optimizer_data))
    {
    }

    [[nodiscard]] std::optional<JsonStrategyConfig> optimize();

    void subscribe_for_passed_check(std::function<void(int, int)> && on_passed_checks)
    {
        if (m_on_passed_check) {
            std::cout << "ERROR: on_passed_check already set" << std::endl;
        }
        m_on_passed_check = std::move(on_passed_checks);
    }

private:
    ByBitGateway & m_gateway;
    Symbol m_symbol;
    Timerange m_timerange;
    JsonStrategyMetaInfo m_optimizer_data;
    std::function<void(unsigned, unsigned)> m_on_passed_check = [](unsigned, unsigned) {};
};
