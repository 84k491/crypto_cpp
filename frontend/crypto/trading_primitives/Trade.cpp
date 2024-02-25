#include "Trade.h"

std::ostream & operator<<(std::ostream & os, const UnsignedVolume & volume)
{
    os << volume.value();
    return os;
}
