#include "MarketDataMessages.h"

#include <nlohmann/json.hpp>

#include <ostream>

void from_json(const nlohmann::json & j, PublicTrade & trade)
{
    j.at("s").get_to(trade.symbol);
    trade.price = std::stod(j.at("p").get<std::string>());
    trade.volume = std::stod(j.at("v").get<std::string>());
    std::string side = j.at("S").get<std::string>();
    trade.side = side == "Buy" ? Side::Buy : Side::Sell;
    size_t timestamp = j.at("T");
    trade.timestamp = std::chrono::milliseconds(timestamp);
}

std::ostream & operator<<(std::ostream & os, const PublicTrade & trade)
{
    os << "PublicTrade\n"
       << "timestamp: " << trade.timestamp.count() << std::endl
       << "symbol: " << trade.symbol << std::endl
       << "price: " << trade.price << std::endl
       << "volume: " << trade.volume << std::endl
       << "side: " << (trade.side == Side::Buy ? "Buy" : "Sell") << std::endl;
    return os;
}

void from_json(const nlohmann::json & j, PublicTradeList & trades)
{
    j.at("topic").get_to(trades.topic);
    j.at("type").get_to(trades.type);
    size_t timestamp = j.at("ts");
    trades.timestamp = std::chrono::milliseconds(timestamp);

    for (const auto & item : j.at("data")) {
        PublicTrade trade;
        item.get_to(trade);
        trades.trades.push_back(trade);
    }
}

std::ostream & operator<<(std::ostream & os, const PublicTradeList & trades)
{
    os << "PublicTradeList\n"
       << "topic: " << trades.topic << std::endl
       << "type: " << trades.type << std::endl
       << "timestamp: " << trades.timestamp.count() << std::endl;
    for (const auto & trade : trades.trades) {
        os << trade << std::endl;
    }
    return os;
}
