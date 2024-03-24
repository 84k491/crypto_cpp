#pragma once

#include "Signal.h"
#include "Tpsl.h"
#include "qcustomplot.h"

class MultiSeriesChart : public QCustomPlot
{
public:
    MultiSeriesChart(QWidget * parent = nullptr);

    void clear();

    void push_series_value(const std::string & series_name,
                           std::chrono::milliseconds ts,
                           double data);
    void push_signal(Signal signal);
    void push_tpsl(std::chrono::milliseconds ts, Tpsl tpsl);

    void set_title(const std::string & title);

private:

    std::map<std::string, int> m_series_indexes;
};
