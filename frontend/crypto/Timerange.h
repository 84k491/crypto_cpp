#include "ohlc.h"

#include <chrono>

class Timerange
{
public:
    Timerange(std::chrono::milliseconds start, std::chrono::milliseconds end);

    bool intersects(const Timerange & other) const;
    bool intersects(std::chrono::milliseconds timestamp) const;

    auto & start() const { return m_start; }
    auto & end() const { return m_end; }

    bool contains(std::chrono::milliseconds timestamp) const;

    std::chrono::milliseconds duration() const;

    bool operator==(const Timerange & other) const;

private:
    std::chrono::milliseconds m_start;
    std::chrono::milliseconds m_end; // included

    std::map<std::chrono::milliseconds, OHLC> m_prices;
};

template <>
struct std::hash<Timerange>
{
    std::size_t operator()(const Timerange & k) const noexcept
    {
        return std::hash<size_t>{}(k.start().count()) ^ std::hash<size_t>{}(k.end().count());
    }
};
