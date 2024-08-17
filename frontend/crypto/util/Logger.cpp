#include "Logger.h"

#include <print>

Logger::Logger()
    : m_event_loop(*this)
{
}

Logger & Logger::i()
{
    static Logger l;
    return l;
}

void Logger::invoke(const std::variant<LogEvent> & var)
{
    std::visit(
            VariantMatcher{
                    [&](const LogEvent & ev) {
                        handle_event(ev);
                    }},
            var);
}

void Logger::handle_event(const LogEvent & ev)
{
    const auto ts = std::chrono::system_clock::now();
    const std::string str = std::format("[{}][{}]: {}",
                                 ts,
                                 to_string(ev.level),
                                 ev.log_str);
    std::cout << str << std::endl;
}

template <LogLevel level>
void Logger::log(std::string && str)
{
    LogEvent ev(level, std::move(str));
    i().m_event_loop.push(ev);
}

template void Logger::log<LogLevel::Debug>(std::string && str);
template void Logger::log<LogLevel::Status>(std::string && str);
template void Logger::log<LogLevel::Info>(std::string && str);
template void Logger::log<LogLevel::Warning>(std::string && str);
template void Logger::log<LogLevel::Error>(std::string && str);
