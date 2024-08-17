#include "LogLevel.h"

#include <sstream>

std::ostream & operator<<(std::ostream & os, LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:
        os << "Debug";
        break;
    case LogLevel::Status:
        os << "Status";
        break;
    case LogLevel::Info:
        os << "Info";
        break;
    case LogLevel::Warning:
        os << "Warning";
        break;
    case LogLevel::Error:
        os << "Error";
        break;
    }
    return os;
}

std::string to_string(LogLevel level)
{
    std::stringstream os;
    os << level;
    return os.str();
}
