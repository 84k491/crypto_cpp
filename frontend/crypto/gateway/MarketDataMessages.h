#include "Enums.h"

#include "nlohmann/json_fwd.hpp"

#include <chrono>
#include <string>

struct PublicTrade
{
    std::chrono::milliseconds timestamp;
    std::string symbol;
    double price;
    double volume;
    Side side;
};
void from_json(const nlohmann::json & j, PublicTrade & trade);
std::ostream & operator<<(std::ostream & os, const PublicTrade & trade);

struct PublicTradeList
{
    std::string topic;
    std::string type;
    std::chrono::milliseconds timestamp;
    std::vector<PublicTrade> trades;
};
void from_json(const nlohmann::json & j, PublicTradeList & trades);
std::ostream & operator<<(std::ostream & os, const PublicTradeList & trades);
