#include "restincurl.h"

#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>

class ByBitGateway
{
public:
    ByBitGateway() = default;

    std::future<std::string> get_klines()
    {
        return std::async(std::launch::async, [&] {
            std::condition_variable cv;
            std::mutex m;
            std::string string_result;

            client
                    .Build()
                    ->Get("https://api-testnet.bybit.com/v5/market/kline?symbol=BTCUSDT&category=spot&interval=15")
                    .WithCompletion([&](const restincurl::Result & result) {
                        std::lock_guard lock(m);
                        string_result = result.body;
                        cv.notify_all();
                    })
                    .Execute();

            std::unique_lock lock(m);
            cv.wait(lock, [&] { return !string_result.empty(); });
            return string_result;
        });
    }

private:
    restincurl::Client client;
};

int main(int argc, char * argv[])
{
    ByBitGateway gateway;
    auto future = gateway.get_klines();
    future.wait();
    std::cout << future.get() << std::endl;
    return 0;
}
