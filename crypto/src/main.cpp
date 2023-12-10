#include "ByBitGateway.h"
#include "StrategyInstance.h"

#include <chrono>

int main(int argc, char * argv[])
{
    const auto start = std::chrono::milliseconds{ 1700554320000};
    const auto end = std::chrono::milliseconds{1700727120000};

    ByBitGateway gateway;

    std::pair<std::chrono::minutes, std::chrono::minutes> slow_limits = {
            std::chrono::minutes{10},
            std::chrono::minutes{100}};
    std::pair<std::chrono::minutes, std::chrono::minutes> fast_limits = {
            std::chrono::minutes{5},
            std::chrono::minutes{50}};

    double max_profit = 0.;
    double best_fast_interval = 0.;
    double best_slow_interval = 0.;
    for (auto slow_interval = slow_limits.first; slow_interval <= slow_limits.second; slow_interval += std::chrono::minutes{1}) {
        for (auto fast_interval = fast_limits.first; fast_interval <= fast_limits.second; fast_interval += std::chrono::minutes{1}) {
            const DoubleSmaStrategyConfig conf = DoubleSmaStrategyConfig{
                    slow_interval,
                    fast_interval};
            if (!conf.is_valid()) {
                continue;
            }

            StrategyInstance instance(
                    Timerange{start, end},
                    conf,
                    gateway);
            instance.run();
            std::cout << "Fast interval: " << fast_interval.count() << "; Slow interval: " << slow_interval.count() << "; Result depo: " << instance.get_strategy_result().final_profit << std::endl;
            if (max_profit < instance.get_strategy_result().final_profit) {
                max_profit = instance.get_strategy_result().final_profit;
                best_fast_interval = static_cast<double>(fast_interval.count());
                best_slow_interval = static_cast<double>(slow_interval.count());
            }
        }
    }

    std::cout << "Best final_profit: " << max_profit << std::endl;
    std::cout << "Best fast interval: " << best_fast_interval << std::endl;
    std::cout << "Best slow interval: " << best_slow_interval << std::endl;

    return 0;
}
