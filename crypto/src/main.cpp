#include "ByBitGateway.h"
#include "StrategyInstance.h"

#include <chrono>

int main(int argc, char * argv[])
{
    const auto start = std::chrono::milliseconds{1700556200000};
    const auto end = std::chrono::milliseconds{1700735400000};

    ByBitGateway gateway;

    StrategyInstance instance(
            Timerange{start, end},
            DoubleSmaStrategyConfig{
                    std::chrono::minutes{25},
                    std::chrono::minutes{5}},
            gateway);
    instance.run();
    std::cout << "Result depo: " << instance.get_strategy_result().profit << std::endl;

    return 0;
}
