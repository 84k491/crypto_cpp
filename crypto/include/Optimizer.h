#pragma once

#include "ByBitGateway.h"
#include "StrategyInstance.h"

#include <nlohmann/json.hpp>

#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

class OptimizerParser
{
public:
    OptimizerParser(nlohmann::json optimizer_data)
        : m_data(std::move(optimizer_data))
    {
    }

    std::vector<nlohmann::json> get_possible_configs();

private:
    const nlohmann::json m_data;
};

template <class StrategyT>
class Optimizer
{
public:
    Optimizer(ByBitGateway & gateway, Timerange timerange, nlohmann::json optimizer_data)
        : m_gateway(gateway)
        , m_timerange(std::move(timerange))
        , m_optimizer_data(std::move(optimizer_data))
    {
    }

    [[nodiscard]] nlohmann::json optimize();

    void subscribe_for_passed_check(std::function<void(int, int)> && on_passed_checks)
    {
        if (m_on_passed_check) {
            std::cout << "ERROR: on_passed_check already set" << std::endl;
        }
        m_on_passed_check = std::move(on_passed_checks);
    }

private:
    ByBitGateway & m_gateway;
    Timerange m_timerange;
    nlohmann::json m_optimizer_data;
    std::function<void(unsigned, unsigned)> m_on_passed_check;
};

template <class StrategyT>
nlohmann::json Optimizer<StrategyT>::optimize()
{
    static_assert(std::is_constructible_v<typename StrategyT::ConfigT, nlohmann::json>);

    OptimizerParser parser(m_optimizer_data);

    double max_profit = -std::numeric_limits<double>::max();
    typename StrategyT::ConfigT best_config("");
    const auto configs = parser.get_possible_configs();
    for (unsigned i = 0; i < configs.size(); ++i) {
        const auto & config_json = configs[i];
        typename StrategyT::ConfigT config(config_json);
        if (!config.is_valid()) {
            continue;
        }
        StrategyInstance strategy_instance(m_timerange, config, m_gateway);
        strategy_instance.run();
        const auto profit = strategy_instance.get_strategy_result().final_profit;
        if (max_profit < profit) {
            max_profit = profit;
            best_config = config;
        }
        m_on_passed_check(i, configs.size());
    }
    return best_config.to_json();
}
