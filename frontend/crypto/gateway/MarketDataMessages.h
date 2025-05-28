#include "Side.h"

#include "nlohmann/json_fwd.hpp"

#include <chrono>
#include <string>

struct ByBitPublicTrade
{
    std::chrono::milliseconds timestamp;
    std::string symbol;
    double price;
    double volume;
    Side side = Side::buy();
};
void from_json(const nlohmann::json & j, ByBitPublicTrade & trade);
std::ostream & operator<<(std::ostream & os, const ByBitPublicTrade & trade);

struct ByBitPublicTradeList
{
    std::string topic;
    std::string type;
    std::chrono::milliseconds timestamp;
    std::vector<ByBitPublicTrade> trades;
};
void from_json(const nlohmann::json & j, ByBitPublicTradeList & trades);
std::ostream & operator<<(std::ostream & os, const ByBitPublicTradeList & trades);
