#include "GridLevels.h"
#include <cmath>

//   ...
// ------------------------ +2 * width
//   1 lvl
// ------------------------ +1 * width
//   0 lvl
// ------------------------ ref
//   0 lvl
// ------------------------ -1 * width
//   -1 lvl
// ------------------------ -2 * width
//   ...
int GridLevels::get_level_number(double price, double trend, double level_width)
{
    const double diff = price - trend;
    const double dbl_num = diff / level_width;
    if (dbl_num > 0) {
        return int(floorl(dbl_num));
    }
    else {
        return int(ceill(dbl_num));
    }
}

double GridLevels::get_price_from_level_number(int level_num, double trend, double level_width)
{
    double offset = level_num * level_width;
    return trend + offset;
}

