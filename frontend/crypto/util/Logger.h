#pragma once

#include "EventLoop.h"
#include "EventLoopSubscriber.h"
#include "Events.h"

#include <fmt/core.h>

#define LOG_DEBUG(FMT, ...)                                                \
    do {                                                                   \
        if (Logger::current_min_log_level() <= LogLevel::Debug) {          \
            Logger::logf<LogLevel::Debug>(FMT __VA_OPT__(, ) __VA_ARGS__); \
        }                                                                  \
    } while (false)

#define LOG_STATUS(FMT, ...)                                                \
    do {                                                                    \
        if (Logger::current_min_log_level() <= LogLevel::Status) {          \
            Logger::logf<LogLevel::Status>(FMT __VA_OPT__(, ) __VA_ARGS__); \
        }                                                                   \
    } while (false)

#define LOG_INFO(FMT, ...)                                                \
    do {                                                                  \
        if (Logger::current_min_log_level() <= LogLevel::Info) {          \
            Logger::logf<LogLevel::Info>(FMT __VA_OPT__(, ) __VA_ARGS__); \
        }                                                                 \
    } while (false)

#define LOG_WARNING(FMT, ...)                                                \
    do {                                                                     \
        if (Logger::current_min_log_level() <= LogLevel::Warning) {          \
            Logger::logf<LogLevel::Warning>(FMT __VA_OPT__(, ) __VA_ARGS__); \
        }                                                                    \
    } while (false)

#define LOG_ERROR(FMT, ...)                                                \
    do {                                                                   \
        if (Logger::current_min_log_level() <= LogLevel::Error) {          \
            Logger::logf<LogLevel::Error>(FMT __VA_OPT__(, ) __VA_ARGS__); \
        }                                                                  \
    } while (false)

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
    static LogLevel current_min_log_level();

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
    EventLoop m_event_loop;

    EventChannel<LogEvent> m_log_channel;

    LogLevel m_min_log_level = LogLevel::Debug;

    EventSubcriber m_sub;
};
