#pragma once

#include "Events.h"

class BybitTradesDownloader
{
    static constexpr std::string_view url_base = "https://public.bybit.com";
    static constexpr std::string_view download_dir = ".download";

public:
    BybitTradesDownloader();

    static std::vector<std::pair<std::chrono::milliseconds, OHLC>> request(const HistoricalMDRequest & req);
};
