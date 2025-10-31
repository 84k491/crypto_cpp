#include "BybitTradesDownloader.h"

#include "DateTimeConverter.h"
#include "Logger.h"

#include <filesystem>
#include <list>

namespace {

std::list<std::string> time_range_to_string_list(HistoricalMDRequestData timerange)
{
    std::list<std::string> res;

    auto current = timerange.start;
    auto end = timerange.end;
    while (end > current) {
        res.push_back(DateTimeConverter::date(current));
        current += std::chrono::hours(24);
    }

    return res;
}

std::list<std::string> csv_file_list(const HistoricalMDRequest & req)
{
    std::list<std::string> res;
    const auto date_list = time_range_to_string_list(req.data);
    for (const auto & date : date_list) {
        // 2024-12-25
        std::string file_name = req.symbol.symbol_name + date + ".csv";
        res.push_back(file_name);
    }

    return res;
}

std::vector<std::string> split_string(const std::string & s, const std::string & delimiter)
{
    size_t pos_start = 0, pos_end = 0;
    const auto delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

} // namespace

FileReader::FileReader(std::string filepath)
    : ifs(filepath)
{
    if (!ifs.is_open()) {
        LOG_ERROR("Can't open file: {}, {}", filepath, errno);
        return;
    }
    std::getline(ifs, last_line); // skip csv header
}

// 0 - timestamp,
// 1 - symbol,
// 2 - side,
// 3 - size,
// 4 - price,
// 5 - tickDirection,
// 6 - trdMatchID,
// 7 - grossValue,
// 8 - homeNotional,
// 9 - foreignNotional
std::optional<PublicTrade> FileReader::get_next()
{
    if (!std::getline(ifs, last_line)) {
        return std::nullopt;
    }

    const auto split_str = split_string(last_line, ",");
    const std::string & ts_str = split_str[0];
    const auto ts_split = split_string(ts_str, ".");
    const auto seconds = std::stoll(ts_split[0]);

    size_t milli = 0;
    if (ts_split.size() > 1) {
        // there can be from 0 to N digits.
        // keeping first 3
        const std::string milli_str = ts_split[1].substr(0, 3);
        // the rest must be multiplied, e.g 1712967661.67 -> multiply by 10 to get 670
        const auto multiplier = std::pow(10, 3 - milli_str.size());
        milli = std::stoll(milli_str) * multiplier;
    }

    const std::chrono::milliseconds ts = std::chrono::seconds{seconds} + std::chrono::milliseconds{milli};

    const std::string & price_str = split_str[4];
    const double price = std::stod(price_str);

    const std::string side_str = split_str[2];
    const Side side = side_str == "Buy" ? Side::buy() : Side::sell();

    const std::string & volume_str = split_str[3];
    const double volume_double = std::stod(volume_str);
    const SignedVolume volume = SignedVolume(UnsignedVolume::from(volume_double).value(), side); // assuming vol is positive

    return {{ts, price, volume}};
}

SequentialMarketDataReader::SequentialMarketDataReader(std::list<std::string> files)
    : m_files(std::move(files))
{
}

std::optional<PublicTrade> SequentialMarketDataReader::get_next()
{
    if (!m_public_trades.empty()) {
        auto res = m_public_trades.front();
        m_public_trades.pop_front();
        return res;
    }

    if (m_files.empty()) {
        return std::nullopt;
    }

    auto reader = FileReader{m_files.front()};
    m_files.pop_front();

    for (auto line = reader.get_next(); line.has_value(); line = reader.get_next()) {
        m_public_trades.push_back(line.value());
    }

    return get_next();
}

std::list<std::string> BybitTradesDownloader::download(const HistoricalMDRequest & req)
{
    if (!std::filesystem::is_directory(download_dir)) {
        std::filesystem::create_directory(download_dir);
    }

    const auto files_list = csv_file_list(req);
    for (const auto & csv_file : files_list) {
        if (!std::filesystem::exists(std::string(download_dir) + "/" + csv_file)) {

            const std::string url = std::string(url_base) +
                    "/trading/" +
                    req.symbol.symbol_name +
                    "/" +
                    csv_file +
                    ".gz";

            LOG_DEBUG("Downloading {}", csv_file);
            std::string curl = "curl -XGET '";
            curl += url;
            curl += "' -o ";
            curl += std::string(download_dir);
            curl += "/";
            curl += csv_file;
            curl += ".gz";
            LOG_DEBUG("Rest request: {}", curl.c_str());
            system(curl.c_str());
            const auto gunzip = "gunzip " + std::string(download_dir) + "/" + csv_file + ".gz";
            LOG_DEBUG("Unzipping: {}", gunzip);
            system(gunzip.c_str());

            if (!std::filesystem::exists(std::string(download_dir) + "/" + csv_file)) {
                LOG_ERROR("Can't download file: {}", csv_file);
                return {};
            }

            LOG_DEBUG("Downloaded file: {}", csv_file);
        }
    }

    std::list<std::string> res;
    for (const auto & csv_file : files_list) {
        res.push_back(std::string{download_dir} + "/" + csv_file);
    }
    return res;
}

std::shared_ptr<SequentialMarketDataReader> BybitTradesDownloader::request(const HistoricalMDRequest & req)
{
    std::vector<std::pair<std::chrono::milliseconds, double>> res;

    const auto files = download(req);
    const auto reader = std::make_shared<SequentialMarketDataReader>(files);

    return reader;
}
