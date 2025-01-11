#pragma once

#include "Ohlc.h"
#include "Tpsl.h"
#include "Trade.h"
#include "TrailingStopLoss.h"
#include "qcustomplot.h"

class MultiSeriesChart : public QCustomPlot
{
public:
    MultiSeriesChart(QWidget * parent = nullptr);

    void clear();

    void push_series_vector(
            const std::string & series_name,
            const std::vector<std::pair<
                    std::chrono::milliseconds,
                    double>> & data);
    void push_series_value(const std::string & series_name,
                           std::chrono::milliseconds ts,
                           double data);

    void push_scatter_series_vector(const std::string & series_name,
                                    const std::vector<std::pair<
                                            std::chrono::milliseconds,
                                            double>> & data);
    void push_trade(const Trade & trade);
    void push_tpsl(std::chrono::milliseconds ts, Tpsl tpsl);
    void push_stop_loss(std::chrono::milliseconds ts, double stop_price);

    void set_title(const std::string & title);

private:
    void push_series_value_dont_replot(const std::string & series_name,
                                       std::chrono::milliseconds ts,
                                       double data,
                                       bool is_scatter);

    QCPGraph * get_graph_for_series(std::string_view series_name, bool is_scatter);

private:
    const std::chrono::milliseconds limit_interval = std::chrono::minutes{60};
    std::unordered_map<std::string, std::chrono::milliseconds> m_last_points; // to limit the rate

    std::map<std::string, int> m_series_indexes;
};
