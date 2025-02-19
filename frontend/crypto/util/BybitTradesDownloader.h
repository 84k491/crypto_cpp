#pragma once

#include "Events.h"

#include <fstream>
#include <list>

class FileReader
{
public:
    FileReader(std::string filepath);

    std::optional<std::pair<std::chrono::milliseconds, double>> get_next();

private:
    std::ifstream ifs;
    std::string last_line;
};

class SequentialMarketDataReader
{
public:
    SequentialMarketDataReader(std::list<std::string> files);

    std::optional<std::pair<std::chrono::milliseconds, double>> get_next();

private:
    std::list<FileReader> m_readers;
};

class BybitTradesDownloader
{
    static constexpr std::string_view url_base = "https://public.bybit.com";
    static constexpr std::string_view download_dir = ".download";

public:
    BybitTradesDownloader();

    static std::vector<std::pair<std::chrono::milliseconds, double>> request(const HistoricalMDRequest & req);
    static std::shared_ptr<SequentialMarketDataReader> request_lowmem(const HistoricalMDRequest & req);

private:
    static std::list<std::string> download(const HistoricalMDRequest & req);
};
