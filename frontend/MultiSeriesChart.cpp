#include "MultiSeriesChart.h"

namespace {
// colors for series
std::vector<QColor> colors = {
        QColor(110, 40, 255),
        QColor(255, 110, 40),
        QColor(40, 255, 110),
        QColor(140, 50, 80),
};
} // namespace

MultiSeriesChart::MultiSeriesChart(QWidget * parent)
    : QCustomPlot(parent)
{
    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), xAxis2, SLOT(setRange(QCPRange)));
    connect(yAxis, SIGNAL(rangeChanged(QCPRange)), yAxis2, SLOT(setRange(QCPRange)));
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
}

void MultiSeriesChart::push_series_value(const std::string & series_name,
                                         std::chrono::milliseconds ts,
                                         double data)
{
    const auto & [it, inserted] = m_series_indexes.try_emplace(series_name, m_series_indexes.size());
    const auto idx = it->second;
    if (inserted) {
        addGraph();
        graph(idx)->setName(series_name.c_str());
        graph(idx)->setPen(QPen(colors[idx]));
    }
    graph(idx)->addData(static_cast<double>(ts.count()), data);
    graph(idx)->rescaleAxes();
    replot();
}

void MultiSeriesChart::clear()
{
    clearGraphs();
    m_series_indexes.clear();
    replot();
}

void MultiSeriesChart::push_signal(Signal signal)
{
}

void MultiSeriesChart::push_tpsl(std::chrono::milliseconds ts, Tpsl tpsl)
{
}

void MultiSeriesChart::set_title(const std::string & title)
{
}
