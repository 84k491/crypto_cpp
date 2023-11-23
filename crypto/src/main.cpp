#include "ByBitGateway.h"
#include "DoubleSmaStrategy.h"

#include <chrono>

int main(int argc, char * argv[])
{
    ByBitGateway gateway;
    const auto start = std::chrono::milliseconds{1700556200000};
    const auto end = std::chrono::milliseconds{1700735400000};
    const auto md_interval = end - start;

    DoubleSmaStrategy strategy(std::chrono::minutes{25}, std::chrono::minutes{5});
    gateway.set_on_kline_received([&](std::pair<std::chrono::milliseconds, OHLC> ts_and_ohlc) {
        const auto & [ts, ohlc] = ts_and_ohlc;
        const auto signal = strategy.push_price({ts, ohlc.close});
        if (signal.has_value()) {
            std::cout << signal.value() << std::endl;
        }
    });
    gateway.get_klines(start, end);

    return 0;
}
