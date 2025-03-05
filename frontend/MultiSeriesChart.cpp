#include "MultiSeriesChart.h"

#include "Logger.h"

namespace {
// colors for series
std::vector<QColor> colors = {
        QColor(110, 40, 255),
        QColor(255, 110, 40),
        QColor(40, 255, 110),
        QColor(140, 50, 80),
};

const auto long_bar_color = QColor(9, 121, 105);
const auto short_bar_color = QColor(255, 87, 51);

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
                {"take_profit", {QCPScatterStyle::ssDiamond, Qt::black, Qt::green, 10}},
                {"stop_loss", {QCPScatterStyle::ssDiamond, Qt::black, Qt::red, 10}},
                {"trailing_stop_loss", {QCPScatterStyle::ssDiamond, Qt::black, Qt::darkYellow, 10}},
                {"fee_profit_price", {QCPScatterStyle::ssCircle, Qt::black, Qt::green, 10}},
                {"no_loss_price", {QCPScatterStyle::ssCircle, Qt::black, Qt::yellow, 10}},
                {"fee_loss_price", {QCPScatterStyle::ssCircle, Qt::black, Qt::red, 10}},
};

} // namespace

MultiSeriesChart::MultiSeriesChart(QWidget * parent)
    : QCustomPlot(parent)
{
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("hh:mm / dd.MM");
    dateTicker->setTickCount(11);
    xAxis->setTicker(dateTicker);
    xAxis->setTickLabelRotation(30.);
    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), xAxis2, SLOT(setRange(QCPRange)));
    connect(yAxis, SIGNAL(rangeChanged(QCPRange)), yAxis2, SLOT(setRange(QCPRange)));
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    m_marginGroup = new QCPMarginGroup(this);
    axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, m_marginGroup);
}

void MultiSeriesChart::push_candle(const Candle &)
{
    // TODO implement
    throw std::runtime_error("Not implemented");
}

void MultiSeriesChart::push_candle_vector(const std::list<Candle> & data)
{
    const auto timeframe = [&]() -> size_t {
        if (data.empty()) {
            return 10;
        }
        return std::chrono::duration_cast<std::chrono::seconds>(data.front().timeframe()).count();
    }();

    if (m_candle_graph == nullptr) {
        m_candle_graph = new QCPFinancial(xAxis, yAxis);
        m_candle_graph->setChartStyle(QCPFinancial::csCandlestick);
        m_candle_graph->setWidth(timeframe * 0.9);
        m_candle_graph->setTwoColored(true);
        m_candle_graph->setBrushPositive(long_bar_color);
        m_candle_graph->setBrushNegative(short_bar_color);
        m_candle_graph->setPenPositive(QPen(QColor(0, 0, 0)));
        m_candle_graph->setPenNegative(QPen(QColor(0, 0, 0)));
        xAxis->setTickLabels(false);
        xAxis->setTicks(false);
    }

    if (m_volumeAxisRect == nullptr) {
        m_volumeAxisRect = new QCPAxisRect(this);
        plotLayout()->addElement(1, 0, m_volumeAxisRect);
        m_volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 100));
        m_volumeAxisRect->axis(QCPAxis::atBottom)->setLayer("axes");
        m_volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer("grid");
        QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
        dateTicker->setDateTimeFormat("hh:mm / dd.MM");
        dateTicker->setTickCount(11);
        m_volumeAxisRect->axis(QCPAxis::atBottom)->setTicker(dateTicker);
        m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(30.);

        plotLayout()->setRowSpacing(0);
        m_volumeAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msBottom);
        m_volumeAxisRect->setMargins(QMargins(0, 0, 0, 0));
        m_volumeAxisRect->setMarginGroup(QCP::msLeft | QCP::msRight, m_marginGroup);

        m_volume_long_bars = new QCPBars(m_volumeAxisRect->axis(QCPAxis::atBottom), m_volumeAxisRect->axis(QCPAxis::atLeft));
        m_volume_short_bars = new QCPBars(m_volumeAxisRect->axis(QCPAxis::atBottom), m_volumeAxisRect->axis(QCPAxis::atLeft));
        m_volume_long_bars->setWidth(timeframe * 0.9);
        m_volume_long_bars->setPen(Qt::NoPen);
        m_volume_long_bars->setBrush(long_bar_color);
        m_volume_short_bars->setWidth(timeframe * 0.9);
        m_volume_short_bars->setPen(Qt::NoPen);
        m_volume_short_bars->setBrush(short_bar_color);
    }

    QVector<QCPFinancialData> fin_data;
    QSharedPointer<QCPBarsDataContainer> long_volume_data(new QCPBarsDataContainer);
    QSharedPointer<QCPBarsDataContainer> short_volume_data(new QCPBarsDataContainer);
    for (const auto & candle : data) {
        const double time = (static_cast<double>(candle.ts().count()) / 1000.) + (timeframe * 0.5); // QCP renders candle from the middle
        fin_data.emplace_back(
                time,
                candle.open(),
                candle.high(),
                candle.low(),
                candle.close());

        auto & container = candle.side() == Side::buy() ? long_volume_data : short_volume_data;
        QCPBarsData d{time, std::fabs(candle.volume())};
        container->add(d);
    }
    m_candle_graph->data()->set(fin_data, true);
    m_volume_long_bars->setData(long_volume_data);
    m_volume_short_bars->setData(short_volume_data);

    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), m_volumeAxisRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
    connect(m_volumeAxisRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), xAxis, SLOT(setRange(QCPRange)));

    rescaleAxes();
    replot();
}

void MultiSeriesChart::push_series_vector(
        const std::string & series_name,
        const std::list<std::pair<
                std::chrono::milliseconds,
                double>> & data)
{
    auto & last_point = m_last_points[series_name];
    for (const auto & [ts, value] : data) {
        if (ts < last_point + limit_interval) {
            continue;
        }
        last_point = ts;
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
    // casting to seconds
    graph->addData(static_cast<double>(ts.count()) / 1000., data);
    if (!is_scatter) {
        graph->rescaleAxes();
    }
}

void MultiSeriesChart::push_series_value(const std::string & series_name,
                                         std::chrono::milliseconds ts,
                                         double data)
{
    push_series_value_dont_replot(series_name, ts, data, false);
    replot();
}

void MultiSeriesChart::push_trade(const Trade & trade)
{
    const std::string & series_name = trade.side() == Side::buy() ? "buy_trade" : "sell_trade";
    push_series_value_dont_replot(series_name, trade.ts(), trade.price(), true);
    replot();
}

void MultiSeriesChart::push_tpsl(std::chrono::milliseconds ts, Tpsl tpsl)
{
    push_series_value_dont_replot("take_profit", ts, tpsl.take_profit_price, true);
    push_series_value_dont_replot("stop_loss", ts, tpsl.stop_loss_price, true);
    replot();
}

void MultiSeriesChart::push_stop_loss(std::chrono::milliseconds ts, double stop_price)
{
    push_series_value_dont_replot("trailing_stop_loss", ts, stop_price, true);
    replot();
}

void MultiSeriesChart::set_title(const std::string &)
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
