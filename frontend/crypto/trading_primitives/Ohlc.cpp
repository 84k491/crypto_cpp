#include "Ohlc.h"

using json = nlohmann::json;
void to_json(json & j, const OHLC & ohlc)
{
    j = json{{"timestamp", ohlc.timestamp.count()},
             {"open", ohlc.open},
             {"high", ohlc.high},
             {"low", ohlc.low},
             {"close", ohlc.close},
             {"volume", ohlc.volume},
             {"turnover", ohlc.turnover}};
}

std::ostream & operator<<(std::ostream & os, const OHLC & ohlc)
{
    const auto j = json(ohlc);
    os << "OHLC\n"
       << std::setw(4) << j;
    return os;
}
