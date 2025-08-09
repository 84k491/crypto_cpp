#pragma once

namespace GridLevels {

int get_level_number(double price, double trend, double level_width);
double get_price_from_level_number(int level_num, double trend, double level_width);

} // namespace GridLevels
