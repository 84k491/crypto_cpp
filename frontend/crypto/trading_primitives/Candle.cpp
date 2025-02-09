#include "Candle.h"

nlohmann::json Candle::to_json() const
{
    return nlohmann::json{
            {"open", m_open},
            {"high", m_high},
            {"low", m_low},
            {"close", m_close},
            {"volume", m_volume}};
}
