#pragma once

#include "Events.h"

#include <fstream>
#include <list>

class FileReader
{
public:
    FileReader(std::string filepath);

    std::optional<PublicTrade> get_next();

private:
    std::ifstream ifs;
    std::string last_line;
};

class SequentialMarketDataReader
{
public:
    SequentialMarketDataReader(std::list<std::string> files);

    std::optional<PublicTrade> get_next();

private:
    std::list<std::string> m_files;
    std::list<PublicTrade> m_public_trades;
};

class BybitTradesDownloader
{
    static constexpr std::string_view url_base = "https://public.bybit.com";
    static constexpr std::string_view download_dir = ".download";

public:
    BybitTradesDownloader();

    static std::shared_ptr<SequentialMarketDataReader> request(const HistoricalMDRequest & req);

private:
    static std::list<std::string> download(const HistoricalMDRequest & req);
};
