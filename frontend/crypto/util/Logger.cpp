#include "Logger.h"

#include "Events.h"
#include "LogLevel.h"

#include <fmt/chrono.h>

Logger::Logger()
    : m_sub(m_event_loop)
{
    m_sub.subscribe(
            m_log_channel,
            [this](const LogEvent & e) { handle_event(e); });
}

Logger & Logger::i()
{
    static Logger l;
    return l;
}

void Logger::handle_event(const LogEvent & ev)
{
    if (ev.level < m_min_log_level) {
        return;
    }
    const std::string str = fmt::format("[{}][{}]: {}",
                                        ev.ts,
                                        to_string(ev.level),
                                        ev.log_str)
                                    .c_str();
    std::cout << str << std::endl;
}

template <LogLevel level>
void Logger::log(std::string && str)
{
    LogEvent ev(level, std::move(str));
    i().m_log_channel.push(std::move(ev));
}

void Logger::set_min_log_level(LogLevel ll)
{
    i().m_min_log_level = ll;
}

LogLevel Logger::current_min_log_level()
{
    return i().m_min_log_level;
}

template void Logger::log<LogLevel::Debug>(std::string && str);
template void Logger::log<LogLevel::Status>(std::string && str);
template void Logger::log<LogLevel::Info>(std::string && str);
template void Logger::log<LogLevel::Warning>(std::string && str);
template void Logger::log<LogLevel::Error>(std::string && str);
