#include "Timerange.h"

Timerange::Timerange(std::chrono::milliseconds start, std::chrono::milliseconds end)
    : m_start(start)
    , m_end(end)
{
}

bool Timerange::intersects(std::chrono::milliseconds timestamp) const
{
    return m_start <= timestamp && timestamp <= m_end;
}

bool Timerange::intersects(const Timerange & other) const
{
    return intersects(other.m_start) ||
            intersects(other.m_end) ||
            other.intersects(m_start) ||
            other.intersects(m_end);
}

bool Timerange::operator==(const Timerange & other) const
{
    return m_start == other.m_start && m_end == other.m_end;
}

std::chrono::milliseconds Timerange::duration() const
{
    return m_end - m_start;
}
