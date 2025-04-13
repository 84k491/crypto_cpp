#pragma once

#include "EventLoopSubscriber.h"
#include "Events.h"

#include <fmt/core.h>

class Logger
{
public:
    template <LogLevel level>
    static void log(std::string && str);

    template <LogLevel level, typename... Args>
    static void logf(std::string_view fmt, Args &&... args)
    {
        str_logf<level>(fmt, to_string(args)...);
    }

    static void set_min_log_level(LogLevel ll);

private:
    Logger();
    static Logger & i();

    template <class T>
    static std::string to_string(const T & v)
    {
        std::stringstream ss;
        ss << v;
        return ss.str();
    }

    template <LogLevel level, typename... Args>
    static void str_logf(std::string_view fmt, Args &&... args)
    {
        static_assert(
                (std::is_same_v<Args, std::string> && ...),
                "Here we accept only strings");
        log<level>(fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

    void handle_event(const LogEvent & ev);

private:
    EventLoopSubscriber<LogEvent> m_event_loop;
    LogLevel m_min_log_level = LogLevel::Debug;
    std::shared_ptr<ISubscription> m_invoker_sub;
};
