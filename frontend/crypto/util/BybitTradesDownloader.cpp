#include "BybitTradesDownloader.h"

#include "Logger.h"

#include <filesystem>
#include <list>

namespace {

std::string time_to_string(const std::chrono::milliseconds & tp)
{
    const time_t t = std::chrono::duration_cast<std::chrono::seconds>(tp).count();
    std::array<char, 30> s{};
    std::strftime(s.data(), s.size(), "%Y-%m-%d", std::gmtime(&t));
    return {s.data()};
}

std::list<std::string> time_range_to_string_list(HistoricalMDRequestData timerange)
{
    std::list<std::string> res;

    auto current = timerange.start;
    auto end = timerange.end;
    while (end > current) {
        res.push_back(time_to_string(current));
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
        Logger::logf<LogLevel::Error>("Can't open file: {}, {}", filepath, errno);
        return;
    }
    std::getline(ifs, last_line); // skip csv header
}

std::optional<std::pair<std::chrono::milliseconds, double>> FileReader::get_next()
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

    return {{ts, price}};
}

SequentialMarketDataReader::SequentialMarketDataReader(std::list<std::string> files)
    : m_files(std::move(files))
{
}

std::optional<std::pair<std::chrono::milliseconds, double>> SequentialMarketDataReader::get_next()
{
    if (m_files.empty()) {
        return std::nullopt;
    }

    if (m_reader == nullptr) {
        m_reader = std::make_unique<FileReader>(m_files.front());
        m_files.pop_front();
    }

    auto next = m_reader->get_next();
    if (next.has_value()) {
        return next;
    }

    m_reader.reset();
    return get_next();
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
std::vector<std::pair<std::chrono::milliseconds, double>> parse_file(const std::string & file_name)
{
    std::vector<std::pair<std::chrono::milliseconds, double>> res;

    FileReader reader(file_name);
    for (auto line = reader.get_next(); line.has_value(); line = reader.get_next()) {
        res.push_back(line.value());
    }

    return res;
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

            Logger::logf<LogLevel::Debug>("Downloading {}", csv_file);
            std::string curl = "curl -XGET '";
            curl += url;
            curl += "' -o ";
            curl += std::string(download_dir);
            curl += "/";
            curl += csv_file;
            curl += ".gz";
            Logger::logf<LogLevel::Debug>("Rest request: {}", curl.c_str());
            system(curl.c_str());
            const auto gunzip = "gunzip " + std::string(download_dir) + "/" + csv_file + ".gz";
            Logger::logf<LogLevel::Debug>("Unzipping: {}", gunzip);
            system(gunzip.c_str());

            if (!std::filesystem::exists(std::string(download_dir) + "/" + csv_file)) {
                Logger::logf<LogLevel::Error>("Can't download file: {}", csv_file);
                return {};
            }

            Logger::logf<LogLevel::Debug>("Downloaded file: {}", csv_file);
        }
    }

    std::list<std::string> res;
    for (const auto & csv_file : files_list) {
        res.push_back(std::string{download_dir} + "/" + csv_file);
    }
    return res;
}

// curl -XGET "https://public.bybit.com/trading/BTCUSDT/BTCUSDT2024-12-25.csv.gz"
std::vector<std::pair<std::chrono::milliseconds, double>> BybitTradesDownloader::BybitTradesDownloader::request(const HistoricalMDRequest & req)
{
    std::vector<std::pair<std::chrono::milliseconds, double>> res;

    const auto files = download(req);
    SequentialMarketDataReader reader(files);

    for (auto line = reader.get_next(); line.has_value(); line = reader.get_next()) {
        res.push_back(line.value());
    }

    Logger::logf<LogLevel::Debug>("Got {} prices", res.size());
    return res;
}

std::shared_ptr<SequentialMarketDataReader> BybitTradesDownloader::request_lowmem(const HistoricalMDRequest & req)
{
    std::vector<std::pair<std::chrono::milliseconds, double>> res;

    const auto files = download(req);
    const auto reader = std::make_shared<SequentialMarketDataReader>(files);

    return reader;
}
