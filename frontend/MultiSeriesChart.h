#pragma once

#include "Ohlc.h"
#include "Signal.h"
#include "Tpsl.h"
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
    void push_signal(Signal signal);
    void push_tpsl(std::chrono::milliseconds ts, Tpsl tpsl);

    void set_title(const std::string & title);

private:
    void push_series_value_dont_replot(const std::string & series_name,
                                       std::chrono::milliseconds ts,
                                       double data,
                                       bool is_scatter);

    QCPGraph * get_graph_for_series(std::string_view series_name, bool is_scatter);

private:
    std::map<std::string, int> m_series_indexes;
};
