#pragma once

#include <ostream>

enum class WorkStatus : uint8_t
{
    Stopped,
    Running,
    Panic,
};

inline std::string to_string(WorkStatus status)
{
    switch (status) {
    case WorkStatus::Stopped: return "Stopped";
    case WorkStatus::Running: return "Running";
    case WorkStatus::Panic: return "Panic";
    }
}

inline std::ostream & operator<<(std::ostream & os, const WorkStatus & status)
{
    os << to_string(status);
    return os;
}
