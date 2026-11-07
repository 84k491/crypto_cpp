#include "MultiSeriesChart.h"

#include "ConditionalOrders.h"
#include "Logger.h"

#include <algorithm>
#include <cmath>
#include <optional>

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
const auto depo_trend_color = QColor(140, 50, 80);

std::map<std::string, QColor> line_series_colors = {
        {"price", QColor(0, 0, 0)},
        {"upper_band", QColor(232, 141, 56)},
        {"lower_band", QColor(52, 99, 194)},
        {"trend", QColor(140, 50, 80)},
};

std::map<MarketState, QColor> market_states_colors = {
        {MarketState::None, QColor(0, 0, 0, 0)},
        {MarketState::DownTrend, QColor(250, 100, 100)},
        {MarketState::UpTrend, QColor(100, 250, 100)},
        {MarketState::NoTrend, QColor(240, 240, 220)},
        {MarketState::HighVolatility, QColor(240, 220, 240)},
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

double to_seconds(std::chrono::milliseconds ts)
{
    return static_cast<double>(ts.count()) / 1000.;
}

QSharedPointer<QCPAxisTickerDateTime> make_date_ticker()
{
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("hh:mm / dd.MM");
    dateTicker->setTickCount(11);
    return dateTicker;
}

} // namespace

MultiSeriesChart::MultiSeriesChart(QWidget * parent)
    : QCustomPlot(parent)
    , m_marginGroup(new QCPMarginGroup(this))
{
    xAxis->setTicker(make_date_ticker());
    xAxis->setTickLabelRotation(30.);
    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), xAxis2, SLOT(setRange(QCPRange)));
    connect(yAxis, SIGNAL(rangeChanged(QCPRange)), yAxis2, SLOT(setRange(QCPRange)));
    setInteractions(QCP::iRangeDrag);
    axisRect()->setRangeDrag(Qt::Horizontal);

    axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, m_marginGroup);
}

void MultiSeriesChart::create_candle_graphs(std::chrono::milliseconds timeframe)
{
    const double timeframe_sec = to_seconds(timeframe);

    m_candle_graph = new QCPFinancial(xAxis, yAxis);
    m_candle_graph->setChartStyle(QCPFinancial::csCandlestick);
    m_candle_graph->setWidth(timeframe_sec * 0.9);
    m_candle_graph->setTwoColored(true);
    m_candle_graph->setBrushPositive(long_bar_color);
    m_candle_graph->setBrushNegative(short_bar_color);
    m_candle_graph->setPenPositive(QPen(QColor(0, 0, 0)));
    m_candle_graph->setPenNegative(QPen(QColor(0, 0, 0)));
    xAxis->setTickLabels(false);
    xAxis->setTicks(false);

    m_volumeAxisRect = new QCPAxisRect(this);
    plotLayout()->addElement(1, 0, m_volumeAxisRect);
    m_volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 100));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setLayer("axes");
    m_volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer("grid");
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTicker(make_date_ticker());
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(30.);
    m_volumeAxisRect->setRangeDrag(Qt::Horizontal);

    plotLayout()->setRowSpacing(0);
    m_volumeAxisRect->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msBottom);
    m_volumeAxisRect->setMargins(QMargins(0, 0, 0, 0));
    m_volumeAxisRect->setMarginGroup(QCP::msLeft | QCP::msRight, m_marginGroup);

    m_volume_long_bars = new QCPBars(m_volumeAxisRect->axis(QCPAxis::atBottom), m_volumeAxisRect->axis(QCPAxis::atLeft));
    m_volume_short_bars = new QCPBars(m_volumeAxisRect->axis(QCPAxis::atBottom), m_volumeAxisRect->axis(QCPAxis::atLeft));
    m_volume_long_bars->setWidth(timeframe_sec * 0.9);
    m_volume_long_bars->setPen(Qt::NoPen);
    m_volume_long_bars->setBrush(long_bar_color);
    m_volume_short_bars->setWidth(timeframe_sec * 0.9);
    m_volume_short_bars->setPen(Qt::NoPen);
    m_volume_short_bars->setBrush(short_bar_color);

    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), m_volumeAxisRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
    connect(m_volumeAxisRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), xAxis, SLOT(setRange(QCPRange)));
    m_volumeAxisRect->axis(QCPAxis::atBottom)->setRange(xAxis->range());
}

void MultiSeriesChart::push_candle(const Candle & candle)
{
    if (m_candle_graph == nullptr) {
        create_candle_graphs(candle.timeframe());
    }

    // CLAUDE, '*' has higher precedence than '+'; add parentheses to explicitly specify the order of operations (fix available) [readability-math-missing-parentheses]
    const double time = to_seconds(candle.ts()) + to_seconds(candle.timeframe()) * 0.5; // QCP renders candle from the middle
    m_candle_graph->addData(time, candle.open(), candle.high(), candle.low(), candle.close());

    auto * bars = candle.side() == Side::buy() ? m_volume_long_bars : m_volume_short_bars;
    bars->addData(time, std::fabs(candle.volume()));
}

void MultiSeriesChart::push_line_point(const std::string & series_name, std::chrono::milliseconds ts, double value)
{
    auto & last_point = m_last_points[series_name];
    if (ts < last_point + limit_interval) {
        return;
    }
    last_point = ts;
    get_graph_for_series(series_name, QCPGraph::LineStyle::lsStepLeft)->addData(to_seconds(ts), value);
}

void MultiSeriesChart::push_marker_point(const std::string & series_name, std::chrono::milliseconds ts, double value)
{
    get_graph_for_series(series_name, QCPGraph::LineStyle::lsNone)->addData(to_seconds(ts), value);
}

void MultiSeriesChart::push_trade(const Trade & trade)
{
    const std::string & series_name = trade.side() == Side::buy() ? "buy_trade" : "sell_trade";
    push_marker_point(series_name, trade.ts(), trade.price());
}

void MultiSeriesChart::push_tpsl(std::chrono::milliseconds ts, TpslPrices tpsl)
{
    push_marker_point("take_profit", ts, tpsl.take_profit_price);
    push_marker_point("stop_loss", ts, tpsl.stop_loss_price);
}

void MultiSeriesChart::push_stop_loss(std::chrono::milliseconds ts, double stop_price)
{
    push_marker_point("trailing_stop_loss", ts, stop_price);
}

void MultiSeriesChart::push_market_state(std::chrono::milliseconds ts, MarketStateRenderObject state)
{
    const auto ts_sec = to_seconds(ts);

    if (state.state != m_prev_market_state && m_prev_market_state != MarketState::None) {
        const auto graphs = get_channel_graphs(m_prev_market_state);

        // fill last value
        graphs.upper->addData(ts_sec, state.upper_limit);
        graphs.lower->addData(ts_sec, state.lower_limit);

        // turn it off
        graphs.upper->addData(ts_sec, std::numeric_limits<double>::quiet_NaN());
        graphs.lower->addData(ts_sec, std::numeric_limits<double>::quiet_NaN());
    }

    if (state.state != MarketState::None) {
        const auto graphs = get_channel_graphs(state.state);
        graphs.upper->addData(ts_sec, state.upper_limit);
        graphs.lower->addData(ts_sec, state.lower_limit);
    }

    m_prev_market_state = state.state;
}

void MultiSeriesChart::clear_all_points()
{
    for (auto & [name, graph] : m_series_graphs) {
        graph->data()->clear();
    }
    if (m_candle_graph != nullptr) {
        m_candle_graph->data()->clear();
        m_volume_long_bars->data()->clear();
        m_volume_short_bars->data()->clear();
    }
    m_last_points.clear();
    m_step_line_extension_keys.clear();
    m_prev_market_state = MarketState::None;
}

void MultiSeriesChart::extend_step_lines_to(std::chrono::milliseconds ts)
{
    const double ts_sec = to_seconds(ts);

    for (auto & [name, graph] : m_series_graphs) {
        if (graph->lineStyle() != QCPGraph::LineStyle::lsStepLeft) {
            continue;
        }
        auto data = graph->data();
        if (const auto it = m_step_line_extension_keys.find(graph); it != m_step_line_extension_keys.end()) {
            data->remove(it->second);
            m_step_line_extension_keys.erase(it);
        }
        if (data->isEmpty()) {
            continue;
        }
        const auto last_point = *(data->constEnd() - 1);
        if (last_point.key >= ts_sec) {
            continue;
        }
        data->add(QCPGraphData(ts_sec, last_point.value));
        m_step_line_extension_keys[graph] = ts_sec;
    }
}

void MultiSeriesChart::drop_points_outside(std::chrono::milliseconds start, std::chrono::milliseconds end)
{
    const double start_sec = to_seconds(start);
    const double end_sec = to_seconds(end);

    for (auto & [name, graph] : m_series_graphs) {
        graph->data()->removeBefore(start_sec);
        graph->data()->removeAfter(end_sec);
    }
    if (m_candle_graph != nullptr) {
        m_candle_graph->data()->removeBefore(start_sec);
        m_candle_graph->data()->removeAfter(end_sec);
        m_volume_long_bars->data()->removeBefore(start_sec);
        m_volume_long_bars->data()->removeAfter(end_sec);
        m_volume_short_bars->data()->removeBefore(start_sec);
        m_volume_short_bars->data()->removeAfter(end_sec);
    }
}

void MultiSeriesChart::set_time_range(std::chrono::milliseconds start, std::chrono::milliseconds end)
{
    xAxis->setRange(to_seconds(start), to_seconds(end));
}

void MultiSeriesChart::rescale_value_axes()
{
    const auto value_range_of_axis = [&](QCPAxis * value_axis) -> std::optional<QCPRange> {
        std::optional<QCPRange> result;
        for (int i = 0; i < plottableCount(); ++i) {
            auto * p = plottable(i);

            if (p->valueAxis() != value_axis || p == m_depo_trend_graph) {
                continue;
            }

            bool found = false;
            const QCPRange range = p->getValueRange(found, QCP::sdBoth);
            if (!found) {
                continue;
            }

            if (!result.has_value()) {
                result = range;
                continue;
            }

            result->lower = std::min(result->lower, range.lower);
            result->upper = std::max(result->upper, range.upper);
        }

        return result;
    };

    if (const auto range = value_range_of_axis(yAxis); range.has_value()) {
        double margin = range->size() * value_margin_factor;

        if (margin <= 0.) {
            margin = std::fabs(range->lower) * value_margin_factor + 1e-9;
        }

        yAxis->setRange(range->lower - margin, range->upper + margin);
    }

    if (m_volumeAxisRect == nullptr) {
        return;
    }
    auto * volume_axis = m_volumeAxisRect->axis(QCPAxis::atLeft);
    if (const auto range = value_range_of_axis(volume_axis); range.has_value()) {
        volume_axis->setRange(0., range->upper * (1. + value_margin_factor));
    }
}

void MultiSeriesChart::set_title(const std::string &)
{
    // TODO implement
}

void MultiSeriesChart::override_depo_trend(
        std::pair<std::chrono::milliseconds, double> first_point,
        std::pair<std::chrono::milliseconds, double> last_point)
{
    if (m_depo_trend_graph == nullptr) {
        m_depo_trend_graph = addGraph();
        m_depo_trend_graph->setName("depo_trend");
        m_depo_trend_graph->setLineStyle(QCPGraph::lsLine);
        auto pen = QPen(depo_trend_color);
        pen.setWidthF(2);
        m_depo_trend_graph->setPen(pen);
    }

    QSharedPointer<QCPGraphDataContainer> c{new QCPGraphDataContainer};
    QCPGraphData f{to_seconds(first_point.first), first_point.second};
    QCPGraphData l{to_seconds(last_point.first), last_point.second};

    c->add(f);
    c->add(l);

    m_depo_trend_graph->setData(c);
}

QCPGraph * MultiSeriesChart::get_graph_for_series(std::string_view series_name, QCPGraph::LineStyle line_style)
{
    const auto it = m_series_graphs.find(std::string(series_name));
    if (it != m_series_graphs.end()) {
        return it->second;
    }

    const auto idx = m_series_graphs.size();
    auto * graph = addGraph();
    m_series_graphs.emplace(std::string(series_name), graph);

    graph->setName(std::string{series_name}.c_str());
    graph->setLineStyle(line_style);
    if (QCPGraph::LineStyle::lsNone == line_style) {
        const auto display_params_pair_it = scatter_series_display_params.find(std::string(series_name));
        if (display_params_pair_it != scatter_series_display_params.end()) {
            const auto & [style, border, fill, size] = display_params_pair_it->second;
            graph->setScatterStyle(QCPScatterStyle(style, border, fill, size));
        }
        else {
            graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::yellow, 15));
        }
    }
    else {
        const auto color_it = line_series_colors.find(std::string(series_name));
        if (color_it != line_series_colors.end()) {
            auto pen = QPen(color_it->second);
            pen.setWidthF(2);
            graph->setPen(pen);
        }
        else {
            auto pen = QPen(colors[idx % colors.size()]);
            pen.setWidthF(2);
            graph->setPen(pen);
        }
    }

    return graph;
}

MultiSeriesChart::ChannelGraphs MultiSeriesChart::get_channel_graphs(MarketState state)
{
    const std::string upper_name = enumToString(state) + "Upper";
    auto * upper = get_graph_for_series(upper_name, QCPGraph::LineStyle::lsStepLeft);

    const std::string lower_name = enumToString(state) + "Lower";
    auto * lower = get_graph_for_series(lower_name, QCPGraph::LineStyle::lsStepLeft);

    if (upper->channelFillGraph() == nullptr) {

        upper->setChannelFillGraph(lower);

        const QColor fill_color = [&]() {
            const auto it = market_states_colors.find(state);

            if (it == market_states_colors.end()) {
                return QColor(0, 0, 0, 120);
            }

            return it->second;
        }();

        upper->setBrush(QBrush{fill_color});
        upper->setPen(QPen{QColor(0, 0, 0, 0)});
        lower->setPen(QPen{QColor(0, 0, 0, 0)});
    }

    return {.upper = upper, .lower = lower};
}
