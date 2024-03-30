#include "MultiSeriesChart.h"

namespace {
// colors for series
std::vector<QColor> colors = {
        QColor(110, 40, 255),
        QColor(255, 110, 40),
        QColor(40, 255, 110),
        QColor(140, 50, 80),
};

std::map<std::string, QColor> line_series_colors = {
        {"price", QColor(0, 0, 0)},
        {"upper_band", QColor::fromString("#e88d38")},
        {"lower_band", QColor::fromString("#3463c2")},
        {"trend", QColor(140, 50, 80)},
};

std::map<std::string, std::tuple<QCPScatterStyle::ScatterShape, QColor, QColor, int>>
        scatter_series_display_params = {
                {"buy_trade", {QCPScatterStyle::ssTriangle, Qt::black, Qt::green, 10}},
                {"sell_trade", {QCPScatterStyle::ssTriangleInverted, Qt::black, Qt::red, 10}},
                {"take_profit", {QCPScatterStyle::ssCircle, Qt::black, Qt::green, 10}},
                {"stop_loss", {QCPScatterStyle::ssDiamond, Qt::black, Qt::red, 10}},
};

} // namespace

MultiSeriesChart::MultiSeriesChart(QWidget * parent)
    : QCustomPlot(parent)
{
    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), xAxis2, SLOT(setRange(QCPRange)));
    connect(yAxis, SIGNAL(rangeChanged(QCPRange)), yAxis2, SLOT(setRange(QCPRange)));
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
}

void MultiSeriesChart::push_series_vector(
        const std::string & series_name,
        const std::vector<std::pair<
                std::chrono::milliseconds,
                double>> & data)
{
    for (const auto & [ts, value] : data) {
        push_series_value_dont_replot(series_name, ts, value, false);
    }
    replot();
}

void MultiSeriesChart::push_series_value_dont_replot(const std::string & series_name,
                                                     std::chrono::milliseconds ts,
                                                     double data,
                                                     bool is_scatter)
{
    auto * graph = get_graph_for_series(series_name, is_scatter);
    graph->addData(static_cast<double>(ts.count()), data);
    graph->rescaleAxes();
}

void MultiSeriesChart::push_series_value(const std::string & series_name,
                                         std::chrono::milliseconds ts,
                                         double data)
{
    push_series_value_dont_replot(series_name, ts, data, false);
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

QCPGraph * MultiSeriesChart::get_graph_for_series(std::string_view series_name, bool is_scatter)
{
    const auto & [it, inserted] = m_series_indexes.try_emplace(std::string(series_name), m_series_indexes.size());
    const auto idx = it->second;
    if (inserted) {
        addGraph();
        graph(idx)->setName(series_name.data());
        if (is_scatter) {
            graph(idx)->setLineStyle(QCPGraph::lsNone);
            const auto display_params_pair_it = scatter_series_display_params.find(std::string(series_name));
            if (display_params_pair_it != scatter_series_display_params.end()) {
                const auto & [style, border, fill, size] = display_params_pair_it->second;
                graph(idx)->setScatterStyle(QCPScatterStyle(style, border, fill, size));
            }
            else {
                graph(idx)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::yellow, 15));
            }
        }
        else {
            const auto color_it = line_series_colors.find(std::string(series_name));
            if (color_it != line_series_colors.end()) {
                auto pen = QPen(color_it->second);
                pen.setWidthF(2);
                graph(idx)->setPen(pen);
            }
            else {
                auto pen = QPen(colors[idx % colors.size()]);
                pen.setWidthF(2);
                graph(idx)->setPen(pen);
            }
        }
    }
    return graph(idx);
}

void MultiSeriesChart::push_scatter_series_vector(const std::string & series_name,
                                                  const std::vector<std::pair<
                                                          std::chrono::milliseconds,
                                                          double>> & data)
{
    for (const auto & [ts, value] : data) {
        push_series_value_dont_replot(series_name, ts, value, true);
    }
    replot();
}
