#include "ByBitGateway.h"
#include "DoubleSmaStrategy.h"
#include "ohlc.h"

#include <iostream>

int main(int argc, char * argv[])
{
    ByBitGateway gateway;
    auto future = gateway.get_klines(
            std::chrono::milliseconds{1700556200000},
            std::chrono::milliseconds{1700735400000});
    future.wait();
    const auto result = future.get();
    for (const auto & [ts, ohlc] : result) {
        std::cout << ohlc << std::endl;
    }

    DoubleSmaStrategy strategy(...);
    return 0;
}
