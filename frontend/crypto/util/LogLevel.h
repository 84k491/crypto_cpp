#pragma once

#include <ostream>

enum class LogLevel
{
    Debug,
    Status,
    Info,
    Warning,
    Error,
};

std::ostream & operator<<(std::ostream & os, LogLevel level);
std::string to_string(LogLevel level);
