#pragma once

#include <ostream>
enum class WorkStatus
{
    Stopped,
    Backtesting,
    Live,
    Panic,
};

inline std::string to_string(WorkStatus status)
{
    switch (status) {
    case WorkStatus::Stopped: return "Stopped";
    case WorkStatus::Backtesting: return "Backtesting";
    case WorkStatus::Live: return "Live";
    case WorkStatus::Panic: return "Panic";
    }
}

inline std::ostream & operator<<(std::ostream & os, const WorkStatus & status)
{
    os << to_string(status);
    return os;
}
