#include "SymbolJsonCodec.h"

#include "PrimitiveJsonCodec.h"

nlohmann::json JsonCodec<Symbol>::encode(const Symbol & object)
{
    return {
            {"symbol_name", object.symbol_name},
            {"lot_size_filter",
             {
                     {"min_qty", object.lot_size_filter.min_qty},
                     {"max_qty", object.lot_size_filter.max_qty},
                     {"qty_step", object.lot_size_filter.qty_step},
             }},
    };
}

std::optional<Symbol> JsonCodec<Symbol>::decode(const nlohmann::json & json)
{
    try {
        const auto & lot_size_filter = json.at("lot_size_filter");

        Symbol symbol;
        symbol.symbol_name = json.at("symbol_name").get<std::string>();
        symbol.lot_size_filter.min_qty = JsonCodec<double>::decode(lot_size_filter.at("min_qty")).value();
        symbol.lot_size_filter.max_qty = JsonCodec<double>::decode(lot_size_filter.at("max_qty")).value();
        symbol.lot_size_filter.qty_step = JsonCodec<double>::decode(lot_size_filter.at("qty_step")).value();
        return symbol;
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
