#include "Candle.h"

nlohmann::json Candle::to_json() const
{
    return nlohmann::json{
            {"ts", m_timestamp.count()},
            {"close_ts", close_ts().count()},
            {"open", m_open},
            {"high", m_high},
            {"low", m_low},
            {"close", m_close},
            {"volume", volume()},
            {"buy_volume", m_buy_taker_volume},
            {"sell_volume", m_sell_taker_volume},
            {"trade_count", m_trade_count}

    };
}

Candle::DisbalanceCoef Candle::volume_imbalance_coef() const
{
    const auto imbalance = volume_imbalance();

    const auto vol = imbalance.as_unsigned_and_side().first.value();
    const auto side = imbalance.as_unsigned_and_side().second;

    return {vol / volume(), side};
}

SignedVolume Candle::volume_imbalance() const
{
    return SignedVolume{
            m_buy_taker_volume - m_sell_taker_volume,
    };
}
