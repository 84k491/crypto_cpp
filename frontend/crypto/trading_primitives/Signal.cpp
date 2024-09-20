#include "Signal.h"

std::ostream & operator<<(std::ostream & os, const Signal & signal)
{
    os << "Signal: " << signal.side << "; timestamp_ms: " << signal.timestamp.count();
    return os;
}

