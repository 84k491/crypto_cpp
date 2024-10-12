#pragma once

#include "EventLoop.h"
#include "Events.h"

class Logger : public IEventInvoker<LogEvent>
{
public:
    template <LogLevel level>
    static void log(std::string && str);

    template <LogLevel level, typename... Args>
    static void logf(std::string_view fmt, Args &&... args)
    {
        str_logf<level>(fmt, to_string(args)...);
    }

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
        log<level>(std::vformat(fmt, std::make_format_args(args...)));
    }

    void invoke(const std::variant<LogEvent> & value) override;
    static void handle_event(const LogEvent & ev);

private:
    EventLoopHolder<LogEvent> m_event_loop;
};
