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
        log<level>(std::vformat(fmt, std::make_format_args(args...)));
    }

private:
    Logger();
    static Logger & i();

    void invoke(const std::variant<LogEvent> & value) override;
    static void handle_event(const LogEvent & ev);

private:
    EventLoop<LogEvent> m_event_loop;
};
