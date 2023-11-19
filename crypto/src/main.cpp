#include "ByBitGateway.h"
#include "ohlc.h"

#include <iostream>

int main(int argc, char * argv[])
{
    ByBitGateway gateway;
    auto future = gateway.get_klines();
    future.wait();
    auto result = future.get();
    for (const auto & ohlc : result) {
        std::cout << ohlc << std::endl;
    }
    return 0;
}
