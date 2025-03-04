#pragma once

#include "Events.h"

#include <fstream>
#include <list>

class FileReader
{
public:
    FileReader(std::string filepath);

    std::optional<CsvPublicTrade> get_next();

private:
    std::ifstream ifs;
    std::string last_line;
};

class SequentialMarketDataReader
{
public:
    SequentialMarketDataReader(std::list<std::string> files);

    std::optional<CsvPublicTrade> get_next();

private:
    std::list<std::string> m_files;
    std::list<CsvPublicTrade> m_public_trades;
};

class BybitTradesDownloader
{
    static constexpr std::string_view url_base = "https://public.bybit.com";
    static constexpr std::string_view download_dir = ".download";

public:
    BybitTradesDownloader();

    static std::vector<CsvPublicTrade> request(const HistoricalMDRequest & req);
    static std::shared_ptr<SequentialMarketDataReader> request_lowmem(const HistoricalMDRequest & req);

private:
    static std::list<std::string> download(const HistoricalMDRequest & req);
};
