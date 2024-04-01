#include "Vwap.h"

Vwap::Vwap(const UnsignedVolume& vol, double price)
    : m_value(price)
    , m_cumulative_volume(vol.value())
{
}

Vwap & Vwap::operator+=(const Vwap & other)
{
    m_value = (m_value * m_cumulative_volume + other.m_value * other.m_cumulative_volume) / (m_cumulative_volume + other.m_cumulative_volume);
    m_cumulative_volume += other.m_cumulative_volume;
    return *this;
}

Vwap & Vwap::operator-=(const Vwap & other)
{
    m_value = (m_value * m_cumulative_volume - other.m_value * other.m_cumulative_volume) / (m_cumulative_volume - other.m_cumulative_volume);
    m_cumulative_volume -= other.m_cumulative_volume;
    return *this;
}
