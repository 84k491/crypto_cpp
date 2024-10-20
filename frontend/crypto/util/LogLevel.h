#pragma once

#include <ostream>

enum class LogLevel
{
    Debug = 0,
    Status = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
};

std::ostream & operator<<(std::ostream & os, LogLevel level);
std::string to_string(LogLevel level);
