#include "Symbol.h"

std::ostream & operator<<(std::ostream & os, const Symbol & symbol)
{
    os << "\nSymbol: {"
       << "\n  symbol_name = " << symbol.symbol_name
       << "\n  lot_size_filter = {"
       << "\n    min_qty = " << symbol.lot_size_filter.min_qty
       << "\n    max_qty = " << symbol.lot_size_filter.max_qty
       << "\n    qty_step = " << symbol.lot_size_filter.qty_step
       << "\n  }"
       << "\n}";
    return os;
}
