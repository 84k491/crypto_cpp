#include "ChartSeriesListJsonCodec.h"

nlohmann::json JsonCodec<ChartSeriesList>::encode(const ChartSeriesList & object)
{
    auto json = nlohmann::json::array();
    for (const auto & name : object.series_names) {
        json.push_back({
                {"chart", name.chart_name},
                {"series", name.series_name},
        });
    }
    return json;
}

std::optional<ChartSeriesList> JsonCodec<ChartSeriesList>::decode(const nlohmann::json & json)
{
    try {
        ChartSeriesList list;
        for (const auto & element : json) {
            list.series_names.push_back({
                    element.at("chart").get<std::string>(),
                    element.at("series").get<std::string>(),
            });
        }
        return list;
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}
