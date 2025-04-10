#include "DateTimeConverter.h"

#include <array>

namespace DateTimeConverter {

std::string date_time(std::chrono::milliseconds ts)
{
    const time_t t = std::chrono::duration_cast<std::chrono::seconds>(ts).count();
    std::array<char, 30> s{};
    std::strftime(s.data(), s.size(), "%Y-%m-%d/%H:%M:%S", std::gmtime(&t));
    return s.data();
}

std::string date(std::chrono::milliseconds ts)
{
    const time_t t = std::chrono::duration_cast<std::chrono::seconds>(ts).count();
    std::array<char, 30> s{};
    std::strftime(s.data(), s.size(), "%Y-%m-%d", std::gmtime(&t));
    return s.data();
}

} // namespace DateTimeConverter
