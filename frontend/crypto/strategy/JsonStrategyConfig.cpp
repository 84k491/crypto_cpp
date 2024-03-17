#include "JsonStrategyConfig.h"

std::ostream & operator<<(std::ostream & os, const JsonStrategyConfig & config)
{
    os << config.get();
    return os;
}
